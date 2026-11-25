// Fieldwalk_FW-01.ino
// Main entry point. All BT audio handled by playback.cpp (proven helix pipeline).
// input.cpp no longer includes audio_player.h — that library is removed.

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <RotaryEncoder.h>
#include "OLED_Driver.h"
#include "GUI_Paint.h"
#include "DEV_Config.h"

#include "config.h"
#include "enum.h"
#include "globals.h"
#include "input.h"
#include "sd_card.h"
#include "utils.h"
#include "music_db.h"
#include "playback.h"
#include "navigation.h"
#include "display.h"
#include "battery.h"

// ── Index build progress shown on OLED ───────────────────────────────────────
static void indexProgress(const String& status) {
    Paint_NewImage(BlackImage, OLED_2IN42_WIDTH, OLED_2IN42_HEIGHT, 0, BLACK);
    Paint_SelectImage(BlackImage);
    Paint_Clear(BLACK);
    Paint_DrawString_EN(0, 5,  "Building Index", &Font12, BLACK, WHITE);
    Paint_DrawLine(0, 18, 127, 18, WHITE, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
    Paint_DrawString_EN(5, 35, "Scanning:", &Font8, BLACK, WHITE);
    Paint_DrawString_EN(5, 50, status.c_str(), &Font8, BLACK, WHITE);
    displayFlush();
}

// Redraw whatever page is showing (used when the battery percentage changed).
static void refreshScreen() {
    switch (currentPage) {
        case PAGE_MAIN:       showMainMenu();      break;
        case PAGE_SONGS:
        case PAGE_ALBUMS:
        case PAGE_ARTISTS:    showBrowseMenu();    break;
        case PAGE_PLAYLISTS:  showPlaylistMenu();  break;
        case PAGE_RECORDINGS: showRecordingMenu(); break;
        case PAGE_BLUETOOTH:  showBluetoothMenu(); break;
        case PAGE_PLAYBACK:   showPlaybackScreen(); break;
        case PAGE_STATS:      break;   // reads the whole SD card; redrawn on its own when opened
        default:              break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("=== Fieldwalk Starting ===");

    // Buttons
    pinMode(BUTTON_IN,     INPUT_PULLUP);
    pinMode(BTN_PREV,      INPUT_PULLUP);
    pinMode(BTN_NEXT,      INPUT_PULLUP);
    pinMode(BTN_PLAY,      INPUT_PULLUP);
    pinMode(BTN_REC,       INPUT_PULLUP);
    pinMode(BTN_RETURN,    INPUT_PULLUP);
    pinMode(PIN_ENCODER_A, INPUT_PULLUP);
    pinMode(PIN_ENCODER_B, INPUT_PULLUP);
    Serial.println("Buttons OK");

    // Battery: ADC setup happens before Bluetooth/audio start
    battery_init();
    battery_printStatus();

    // Rotary encoder
    encoder = new RotaryEncoder(PIN_ENCODER_A, PIN_ENCODER_B,
                                RotaryEncoder::LatchMode::TWO03);
    attachInterrupt(digitalPinToInterrupt(PIN_ENCODER_A), checkPosition, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_ENCODER_B), checkPosition, CHANGE);
    Serial.println("Encoder OK");

    // OLED
    System_Init();
    delay(100);
    OLED_2IN42_Init();
    delay(100);
    OLED_2IN42_Clear();

    UWORD size = ((OLED_2IN42_WIDTH % 8 == 0)
                  ? (OLED_2IN42_WIDTH / 8)
                  : (OLED_2IN42_WIDTH / 8 + 1)) * OLED_2IN42_HEIGHT;
    BlackImage = (UBYTE*)malloc(size);
    if (!BlackImage) { Serial.println("ERROR: No memory!"); while(1) delay(1000); }
    Serial.println("OLED OK");

    showBoot();

    // SD Card
    Serial.println("Init SD...");
    sdCardReady = initSDCard();
    if (!sdCardReady) {
        Serial.println("SD card FAILED");
    } else {
        Serial.println("SD OK");
        if (!loadMusicIndex()) {
            Serial.println("No index — building...");
            buildMusicIndex(indexProgress);
        }
        Serial.printf("Index ready: %d artists, %d tracks\n",
                      getArtists().size(), getTotalTrackCount());
    }

    // Playback engine — safe, no hardware init yet
    playback_init();

    // Start with artist list
    loadArtists();

    showMainMenu();
}

// ─────────────────────────────────────────────────────────────────────────────
void loop() {
    // Keep audio flowing — decoder runs here on Core 1, no FreeRTOS task needed
    playback_loop();

    battery_update();       // one ADC conversion per visit, never blocks audio
    // 't' on the serial monitor prints battery details (for calibrating the divider)
    while (Serial.available()) {
        int c = Serial.read();
        if (c == 't' || c == 'T') battery_printStatus();
    }
    if (battery_takeChanged()) refreshScreen();     // the percentage changed: redraw the current page

    int encoderChange = getEncoderChange();

    switch (currentPage) {

    // ── MAIN MENU ─────────────────────────────────────────────────────────────
    case PAGE_MAIN:
        if (encoderChange != 0) {
            menuItem += encoderChange;
            if (menuItem < 1) menuItem = 1;
            if (menuItem > 7) menuItem = 7;
            showMainMenu();
        }
        if (encoderPressed()) {
            currentPage = (PageEnum)menuItem;
            selectedItemIndex = 0;
            scrollOffset = 0;
            switch (currentPage) {
            case PAGE_SONGS:
            case PAGE_ALBUMS:
            case PAGE_ARTISTS:
                loadArtists();
                currentBrowseLevel = LEVEL_ARTISTS;
                showBrowseMenu();
                break;
            case PAGE_PLAYLISTS:  showPlaylistMenu();  break;
            case PAGE_RECORDINGS: showRecordingMenu(); break;
            case PAGE_BLUETOOTH:  showBluetoothMenu(); break;
            case PAGE_STATS:      showStatMenu();      break;
            default: break;
            }
        }
        break;

    // ── BROWSE (Artists → Albums → Songs) ────────────────────────────────────
    case PAGE_SONGS:
    case PAGE_ARTISTS:
        if (encoderChange != 0 && !currentList.empty()) {
            selectedItemIndex += encoderChange;
            selectedItemIndex = max(0, min(selectedItemIndex, (int)currentList.size() - 1));
            if (selectedItemIndex < scrollOffset)
                scrollOffset = selectedItemIndex;
            else if (selectedItemIndex >= scrollOffset + MAX_VISIBLE_ITEMS)
                scrollOffset = selectedItemIndex - MAX_VISIBLE_ITEMS + 1;
            showBrowseMenu();
        }

        if (encoderPressed() && !currentList.empty()) {
            String selected = currentList[selectedItemIndex];
            switch (currentBrowseLevel) {
            case LEVEL_ARTISTS:
                currentArtist = selected;
                loadAlbums();
                showBrowseMenu();
                break;
            case LEVEL_ALBUMS:
                currentAlbum = selected;
                loadSongs();
                showBrowseMenu();
                break;
            case LEVEL_SONGS:
                if (selected.indexOf("Play All") >= 0) {
                    returnToPage = currentPage;
                    auto q = buildAlbumQueue(currentArtist, currentAlbum);
                    playback_startQueue(q, 0);
                    currentPage = PAGE_PLAYBACK;
                    showPlaybackScreen();
                } else {
                    int trackIdx = selectedItemIndex - 1;  // -1 for "Play All"
                    auto q = buildAlbumQueue(currentArtist, currentAlbum);
                    if (trackIdx >= 0 && trackIdx < (int)q.size()) {
                        returnToPage = currentPage;
                        playback_startQueue(q, trackIdx);
                        currentPage = PAGE_PLAYBACK;
                        showPlaybackScreen();
                    }
                }
                break;
            }
        }

        if (playPressed() && !currentList.empty()) {
            if (currentBrowseLevel == LEVEL_ALBUMS) {
                currentAlbum = currentList[selectedItemIndex];
                returnToPage = currentPage;
                auto q = buildAlbumQueue(currentArtist, currentAlbum);
                playback_startQueue(q, 0);
                currentPage = PAGE_PLAYBACK;
                showPlaybackScreen();
            } else if (currentBrowseLevel == LEVEL_SONGS) {
                int trackIdx = selectedItemIndex - 1;
                auto q = buildAlbumQueue(currentArtist, currentAlbum);
                if (trackIdx >= 0 && trackIdx < (int)q.size()) {
                    returnToPage = currentPage;
                    playback_startQueue(q, trackIdx);
                    currentPage = PAGE_PLAYBACK;
                    showPlaybackScreen();
                }
            }
        }

        if (returnPressed()) {
            if (currentBrowseLevel == LEVEL_SONGS) {
                currentAlbum = "";
                loadAlbums();
                showBrowseMenu();
            } else if (currentBrowseLevel == LEVEL_ALBUMS) {
                currentArtist = "";
                loadArtists();
                showBrowseMenu();
            } else {
                currentPage = PAGE_MAIN;
                showMainMenu();
            }
        }
        break;

    case PAGE_ALBUMS:
        if (returnPressed()) { currentPage = PAGE_MAIN; showMainMenu(); }
        break;

    case PAGE_PLAYLISTS:
    case PAGE_RECORDINGS:
        if (returnPressed()) { currentPage = PAGE_MAIN; showMainMenu(); }
        break;

    // ── PLAYBACK ──────────────────────────────────────────────────────────────
    case PAGE_PLAYBACK: {
        static unsigned long lastUpdate = 0;
        if (millis() - lastUpdate > 1000) {
            isPlaying       = playback_isPlaying();
            currentQueueIndex = playback_queueIndex();
            currentSongName = playback_currentTitle();
            showPlaybackScreen();
            lastUpdate = millis();
        }

        if (prevPressed()) { playback_previous(); showPlaybackScreen(); }
        if (nextPressed()) { playback_next();     showPlaybackScreen(); }
        if (playPressed()) { playback_togglePause(); }

        if (returnPressed()) {
            playback_stop();
            isPlaying = false;

            String savedArtist = currentArtist;
            String savedAlbum  = currentAlbum;
            currentPage = returnToPage;

            if (!savedArtist.isEmpty() && !savedAlbum.isEmpty()) {
                currentArtist = savedArtist;
                currentAlbum  = savedAlbum;
                loadSongs();
            } else {
                loadArtists();
            }
            showBrowseMenu();
        }
        break;
    }

    // ── BLUETOOTH MENU ────────────────────────────────────────────────────────
    case PAGE_BLUETOOTH:
        if (encoderChange != 0 && !bluetoothDevices.empty()) {
            selectedBluetoothDevice += encoderChange;
            selectedBluetoothDevice = max(0, min(selectedBluetoothDevice,
                                                  (int)bluetoothDevices.size() - 1));
            showBluetoothMenu();
        }

        if (encoderPressed()) {
            // Step 1: Enable BT if off
            if (!playback_isBluetoothEnabled()) {
                showBluetoothMenuScanning();
                playback_enableBluetooth();
                playback_btScan();
                showBluetoothMenu();
                break;
            }
            // Step 2: Scan if no devices found yet
            if (bluetoothDevices.empty()) {
                showBluetoothMenuScanning();
                playback_btScan();
                showBluetoothMenu();
                break;
            }
            // Step 3: Connect to selected device
            if (!playback_isBluetoothConnected()) {
                showBluetoothMenuScanning();
                playback_btConnect(selectedBluetoothDevice);
                showBluetoothMenu();
                break;
            }
            Serial.println("BT: Already connected");
        }

        if (returnPressed()) {
            currentPage = PAGE_MAIN;
            showMainMenu();
        }
        break;

    // ── STATS ─────────────────────────────────────────────────────────────────
    case PAGE_STATS:
        if (playPressed() && sdCardReady) {
            buildMusicIndex(indexProgress);
            loadArtists();
            showStatMenu();
        }
        if (returnPressed()) { currentPage = PAGE_MAIN; showMainMenu(); }
        break;

    default:
        break;
    }
}
