// playback.cpp
// BT audio pipeline:
//   SD file → AudioDecoder (MP3 / WAV / FLAC / AAC) → PcmFeeder (→ 44.1 kHz stereo)
//           → int16 ring buffer → A2DP callback
// Same structure as the proven MP3-only pipeline; the decoder is now chosen per file
// (see audio_decoder.h). No AudioTools dependency.
//
// Key design:
//   - playback_loop() is called from Arduino loop() on Core 1 — no FreeRTOS task needed
//   - Decoding runs in loop() so there is no vTaskDelay stutter
//   - SD reads happen synchronously in the decode path (proven safe at 4MHz HSPI)
//   - BT stack runs on Core 0 as normal
//   - ssid_cb lets the library handle discovery; we match by stored name or MAC

#include "playback.h"
#include "globals.h"
#include "config.h"
#include <SD.h>
#include "BluetoothA2DPSource.h"
#include "esp_gap_bt_api.h"
#include "audio_decoder.h"
#include "pcm_pipeline.h"
#if defined(ARDUINO_ARCH_ESP32)
  #include "esp_heap_caps.h"
#endif

// ── PCM ring buffer (interleaved stereo, 44.1 kHz) ────────────────────────────
static const int RING_FRAMES = 4096;               // 93 ms at 44.1 kHz (same as the original 8192 samples)
typedef PcmRing Ring;
static Ring                g_ring;
static PcmFeeder<Ring>     g_feeder(g_ring);

// Allocate the ring storage once. Spare instruction RAM (IRAM) can hold data too, as long as it is only
// accessed in whole 32-bit words - which is exactly how PcmRing stores a frame (never memset/memcpy it!).
// Using it keeps 16 KB of the ordinary heap free for the decoders and the Bluetooth stack. If there is not
// enough spare IRAM it falls back to the normal heap, which costs exactly what the old static buffer did.
static bool ringInit() {
    if (g_ring.ready()) return true;
    const size_t bytes = (size_t)RING_FRAMES * sizeof(uint32_t);
    uint32_t* mem = nullptr;
    const char* where = "heap";
#if defined(ARDUINO_ARCH_ESP32)
    mem = (uint32_t*)heap_caps_malloc(bytes, MALLOC_CAP_32BIT | MALLOC_CAP_EXEC);
    if (mem) where = "spare IRAM";
#endif
    if (!mem) mem = (uint32_t*)malloc(bytes);
    if (!mem) { Serial.println("Audio ring buffer: out of memory!"); return false; }
    g_ring.begin(mem, RING_FRAMES);
    Serial.printf("Audio ring buffer: %u bytes in %s\n", (unsigned)bytes, where);
    return true;
}

// ── Decoder state ────────────────────────────────────────────────────────────
static AudioDecoder* g_decoder   = nullptr;
static bool          g_trackDone = false;

static void releaseDecoder() {
    if (g_decoder) { delete g_decoder; g_decoder = nullptr; }
}

// ── Diagnostics (shown in the 3-second debug line; reset for every track) ───────
static volatile uint32_t g_dUnderFrames = 0;   // frames of silence the BT callback had to insert (ring empty)
static volatile uint32_t g_dUnderEvents = 0;   // how many separate times that happened
static volatile uint32_t g_dCbGapMaxUs  = 0;   // longest time between two BT callbacks
static uint32_t          g_dLoopGapMaxUs = 0;  // longest time between two playback_loop() calls
static uint32_t          g_dDecodeMaxUs  = 0;  // longest single decode() call (SD read + decoding)
static uint32_t          g_dLastLoopUs   = 0;
static void diagReset() {
    g_dUnderFrames = g_dUnderEvents = g_dCbGapMaxUs = 0;
    g_dLoopGapMaxUs = g_dDecodeMaxUs = g_dLastLoopUs = 0;
}

// Decode until the ring buffer is full (or the track ends). Runs on Core 1.
static void pumpDecoder() {
    if (!g_decoder) return;
    for (;;) {
        if (!g_feeder.drain()) return;          // ring full — the BT callback will make room
        if (g_trackDone) return;

        PcmChunk chunk = {nullptr, 0, 0, 0};
        uint32_t t0 = micros();
        DecodeStatus st = g_decoder->decode(chunk);
        uint32_t dt = micros() - t0;
        if (dt > g_dDecodeMaxUs) g_dDecodeMaxUs = dt;
        if (st != DEC_OK) {
            if (st == DEC_ERROR) Serial.println("Decode error — ending track");
            g_trackDone = true;
            return;
        }
        g_feeder.stage(chunk);
    }
}

// ── Queue / playback state ────────────────────────────────────────────────────
static std::vector<QueueEntry> g_queue;
static int  g_queueIndex = 0;
static bool g_playing    = false;
static bool g_paused     = false;
static PlaybackMode g_mode = PLAYBACK_NONE;

// ── Bluetooth state ───────────────────────────────────────────────────────────
static BluetoothA2DPSource g_a2dp;
static bool   g_btInitialised = false;
static bool   g_btPaused      = true;
static bool   g_btScanDone    = false;
static String g_targetName    = "";
static esp_bd_addr_t g_targetMAC = {0,0,0,0,0,0};
static bool   g_hasTargetMAC  = false;

// ── A2DP data callback (called by BT stack on Core 0) ────────────────────────
static int32_t bt_data_cb(uint8_t* data, int32_t bytes) {
    if (g_btPaused) { memset(data, 0, bytes); return bytes; }
    int16_t* out = (int16_t*)data;
    int frames = bytes / 4;

    static uint32_t lastCbUs = 0;
    uint32_t nowUs = micros();
    if (lastCbUs && nowUs - lastCbUs > g_dCbGapMaxUs) g_dCbGapMaxUs = nowUs - lastCbUs;
    lastCbUs = nowUs;

    bool starved = false;
    for (int i = 0; i < frames; i++) {
        if (!g_ring.readFrame(out[i*2], out[i*2+1])) {
            out[i*2] = out[i*2+1] = 0;
            g_dUnderFrames = g_dUnderFrames + 1;
            starved = true;
        }
    }
    if (starved) g_dUnderEvents = g_dUnderEvents + 1;
    return bytes;
}

static void bt_conn_cb(esp_a2d_connection_state_t state, void*) {
    bluetoothConnected = (state == ESP_A2D_CONNECTION_STATE_CONNECTED);
    Serial.println(bluetoothConnected ? "BT: Connected" : "BT: Disconnected");
}

// ssid_cb — library calls this for each found device; return true to connect
static bool ssid_cb(const char* ssid, esp_bd_addr_t addr, int rssi) {
    if (bluetoothConnected) return false;  // already connected, ignore
    Serial.printf("BT found: %s RSSI:%d\n", ssid, rssi);
    if (g_hasTargetMAC && memcmp(addr, g_targetMAC, ESP_BD_ADDR_LEN) == 0) {
        Serial.println("BT: MAC match");
        return true;
    }
    if (g_targetName.length() > 0 && String(ssid) == g_targetName) {
        memcpy(g_targetMAC, addr, ESP_BD_ADDR_LEN);
        g_hasTargetMAC = true;
        Serial.println("BT: Name match");
        return true;
    }
    return false;
}

// GAP callback for manual scan (BT menu)
static void gap_scan_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* param) {
    if (event == ESP_BT_GAP_DISC_RES_EVT) {
        for (int i = 0; i < param->disc_res.num_prop; i++) {
            if (param->disc_res.prop[i].type == ESP_BT_GAP_DEV_PROP_BDNAME) {
                String name = String((char*)param->disc_res.prop[i].val);
                name.trim();
                if (name.length() > 0) {
                    bool dupe = false;
                    for (auto& d : bluetoothDevices)
                        if (d == name) { dupe = true; break; }
                    if (!dupe) {
                        bluetoothDevices.push_back(name);
                        Serial.println("BT scan: " + name);
                    }
                }
            }
        }
    } else if (event == ESP_BT_GAP_DISC_STATE_CHANGED_EVT) {
        if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
            g_btScanDone = true;
            Serial.printf("Scan done: %d devices\n", bluetoothDevices.size());
        }
    }
}

// ── Open a track file and reset decoder ──────────────────────────────────────
static bool openTrack(const String& path) {
    g_btPaused = true;                  // BT callback outputs silence while the ring is reset
    if (!ringInit()) return false;
    releaseDecoder();                   // free the previous decoder's memory first
    g_trackDone = false;
    g_ring.clear();
    g_feeder.reset();
    diagReset();

    g_decoder = audioDecoderCreate(audioFormatFromPath(path.c_str()));
    if (!g_decoder) { Serial.println("Unsupported file type: " + path); return false; }
    if (!g_decoder->open(path.c_str())) {
        releaseDecoder();
        Serial.println("Cannot play: " + path);
        Serial.printf("  (heap free=%u, largest block=%u)\n",
                      (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
        return false;
    }

    // Pre-fill buffer before allowing BT stack to drain it
    // This eliminates the stutter at track start
    Serial.println("Pre-filling...");
    pumpDecoder();
    Serial.printf("Playback: %s [%s] (buf=%d, heap free=%u, largest block=%u)\n", path.c_str(),
                  g_decoder->name(), g_ring.available(),
                  (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
    return true;
}

// Start the track at g_queueIndex. If it can't be played (unsupported format, unreadable
// or corrupt file) move on in direction 'dir' until one works or every track has failed.
static bool startQueueTrack(int dir) {
    int n = (int)g_queue.size();
    for (int tries = 0; tries < n; tries++) {
        currentSongPath = g_queue[g_queueIndex].path;
        currentSongName = g_queue[g_queueIndex].title;
        if (openTrack(currentSongPath)) {
            g_playing = true; g_paused = false; g_btPaused = false;
            return true;
        }
        g_queueIndex = (g_queueIndex + dir + n) % n;
    }
    Serial.println("No playable track in queue");
    g_playing = false; g_paused = false; g_btPaused = true;
    return false;
}


// ── Public API ────────────────────────────────────────────────────────────────
void playback_init() {
    g_mode = PLAYBACK_NONE;
    g_playing = g_paused = false;
    ringInit();
}

void playback_setI2SPins(int bclk, int lrc, int din) {
    // Reserved for future I2S support
}

void playback_startQueue(const std::vector<QueueEntry>& tracks, int startIndex) {
    g_queue      = tracks;
    g_queueIndex = startIndex;
    if (g_queue.empty()) return;

    g_mode = PLAYBACK_BLUETOOTH;
    if (!g_btInitialised) playback_enableBluetooth();

    if (g_queueIndex < 0 || g_queueIndex >= (int)g_queue.size()) g_queueIndex = 0;

    startQueueTrack(+1);
}

void playback_loop() {
    if (g_mode != PLAYBACK_BLUETOOTH || !g_playing || g_btPaused) return;

    if (g_trackDone) {
        playback_next();
        return;
    }

    uint32_t nowUs = micros();
    if (g_dLastLoopUs && nowUs - g_dLastLoopUs > g_dLoopGapMaxUs) g_dLoopGapMaxUs = nowUs - g_dLastLoopUs;
    g_dLastLoopUs = nowUs;

    // Debug — remove once confirmed working
    static unsigned long lastLog = 0;
    if (millis() - lastLog > 3000) {
        lastLog = millis();
        Serial.printf("decode: buf=%d decoder=%s trackDone=%d paused=%d\n",
            g_ring.available(), g_decoder ? g_decoder->name() : "none", g_trackDone, g_btPaused);
        // under = silence inserted because the ring ran dry; the *Max values are the worst case since the last line
        Serial.printf("  under=%u frames in %u events | BT callback gap max=%ums | loop gap max=%ums | decode() max=%ums\n",
            (unsigned)g_dUnderFrames, (unsigned)g_dUnderEvents, (unsigned)(g_dCbGapMaxUs / 1000),
            (unsigned)(g_dLoopGapMaxUs / 1000), (unsigned)(g_dDecodeMaxUs / 1000));
        Serial.printf("  heap: free=%u  largest block=%u  lowest ever=%u\n",
            (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap(), (unsigned)ESP.getMinFreeHeap());
        g_dCbGapMaxUs = 0; g_dLoopGapMaxUs = 0; g_dDecodeMaxUs = 0;
    }

    // Fill the PCM buffer — runs continuously in Arduino loop() on Core 1.
    // No vTaskDelay here — that's what eliminates the stutter.
    pumpDecoder();
}

void playback_next() {
    if (g_queue.empty()) return;
    g_queueIndex = (g_queueIndex + 1) % (int)g_queue.size();
    startQueueTrack(+1);
}

void playback_previous() {
    if (g_queue.empty()) return;
    if (--g_queueIndex < 0) g_queueIndex = (int)g_queue.size() - 1;
    startQueueTrack(-1);
}

void playback_togglePause() {
    g_btPaused = !g_btPaused;
    g_paused   =  g_btPaused;
    g_playing  = !g_btPaused;
}

void playback_stop() {
    g_playing = false; g_paused = false; g_btPaused = true;
    releaseDecoder();
    g_mode = PLAYBACK_NONE;
}

bool playback_isPlaying()   { return g_playing && !g_paused; }
bool playback_isPaused()    { return g_paused; }
PlaybackMode playback_currentMode() { return g_mode; }
int  playback_queueIndex()  { return g_queueIndex; }
int  playback_queueSize()   { return (int)g_queue.size(); }
String playback_currentTitle() { return g_queue.empty() ? "" : g_queue[g_queueIndex].title; }
String playback_currentPath()  { return g_queue.empty() ? "" : g_queue[g_queueIndex].path; }

// ── Bluetooth management ──────────────────────────────────────────────────────
void playback_enableBluetooth() {
    if (g_btInitialised) { bluetoothEnabled = true; return; }

    Serial.println("BT: Initialising...");
    g_a2dp.set_data_callback(bt_data_cb);
    g_a2dp.set_on_connection_state_changed(bt_conn_cb, nullptr);
    g_a2dp.set_ssid_callback(ssid_cb);
    g_a2dp.start("");   // ssid_cb controls which device to connect to

    g_btInitialised    = true;
    g_btPaused         = true;
    bluetoothEnabled   = true;
    bluetoothConnected = false;
    Serial.println("BT: Ready");
    Serial.printf("Heap after BT init: free=%u, largest block=%u\n",
                  (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
}

void playback_disableBluetooth() {
    g_btPaused       = true;
    bluetoothEnabled = false;
    Serial.println("BT: Paused");
}

bool playback_isBluetoothEnabled()   { return bluetoothEnabled; }
bool playback_isBluetoothConnected() { return bluetoothConnected; }

void playback_btScan() {
    if (!g_btInitialised) playback_enableBluetooth();

    bluetoothDevices.clear();
    selectedBluetoothDevice = 0;
    g_btScanDone = false;

    // Install a scan-only ssid_cb that collects names but doesn't connect
    g_a2dp.set_ssid_callback([](const char* ssid, esp_bd_addr_t addr, int rssi) -> bool {
        Serial.printf("BT scan: %s RSSI:%d\n", ssid, rssi);
        String name = String(ssid);
        name.trim();
        if (name.length() > 0) {
            bool dupe = false;
            for (auto& d : bluetoothDevices)
                if (d == name) { dupe = true; break; }
            if (!dupe) bluetoothDevices.push_back(name);
        }
        return false;  // don't connect during scan
    });

    // Let the library trigger its own discovery
    esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 8, 0);

    // Wait up to 10s for results
    unsigned long start = millis();
    while (millis() - start < 10000) {
        delay(100);
        if (bluetoothDevices.size() > 0 && millis() - start > 3000) break;
    }
    g_btScanDone = true;
    Serial.printf("Scan done: %d devices\n", bluetoothDevices.size());
}

void playback_btConnect(int deviceIndex) {
    if (deviceIndex < 0 || deviceIndex >= (int)bluetoothDevices.size()) return;

    g_targetName   = bluetoothDevices[deviceIndex];
    g_hasTargetMAC = false;
    bluetoothDeviceName = g_targetName;
    Serial.println("BT: Connecting to: " + g_targetName);

    // Restore the connect ssid_cb
    g_a2dp.set_ssid_callback(ssid_cb);

    // Trigger discovery — ssid_cb will connect when target is found
    esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0);
}
