// decoder_flac.cpp — FLAC via dr_flac (public domain, https://github.com/mackron/dr_libs)
//
// Memory: dr_flac allocates about (max block size x channels x 4) bytes when a file is opened,
// i.e. roughly 45 KB for normal 16-bit/44.1 kHz stereo FLAC. It is freed again as soon as the
// track ends, so only one decoder (MP3 / FLAC / AAC) is ever alive at a time.
// 24-bit files are reduced to 16-bit; any sample rate is resampled to 44.1 kHz by the player.

#define DR_FLAC_IMPLEMENTATION
#define DR_FLAC_NO_STDIO            // we read through the SD library, not stdio
#define DR_FLAC_NO_OGG
#define DR_FLAC_NO_CRC              // skip per-frame CRC checks: faster on the ESP32
#define DR_FLAC_NO_SIMD
#define DR_FLAC_BUFFER_SIZE 1024    // default 4096: saves ~3 KB of heap AND of stack inside drflac_open()
#define DRFLAC_ASSERT(expression) ((void)0)
#include "dr_flac.h"

#include "audio_decoder.h"
#include <new>

namespace {

size_t flacRead(void* user, void* buf, size_t bytes) {
    return ((File*)user)->read((uint8_t*)buf, bytes);
}

drflac_bool32 flacSeek(void* user, int offset, drflac_seek_origin origin) {
    File* f = (File*)user;
    int64_t base   = (origin == DRFLAC_SEEK_CUR) ? (int64_t)f->position() : 0;
    int64_t target = base + offset;
    if (target < 0 || target > (int64_t)f->size()) return DRFLAC_FALSE;
    return f->seek((uint32_t)target) ? DRFLAC_TRUE : DRFLAC_FALSE;
}

drflac_bool32 flacTell(void* user, drflac_int64* cursor) {
    *cursor = (drflac_int64)((File*)user)->position();
    return DRFLAC_TRUE;
}

class FlacDecoder : public AudioDecoder {
public:
    ~FlacDecoder() override { closeAll(); }

    bool open(const char* path) override {
        file_ = SD.open(path);
        if (!file_) { Serial.printf("FLAC: cannot open %s\n", path); return false; }

        flac_ = drflac_open(flacRead, flacSeek, flacTell, &file_, nullptr);
        if (!flac_) { Serial.println("FLAC: invalid file or out of memory"); closeAll(); return false; }
        if (flac_->channels < 1 || flac_->sampleRate < 4000) { closeAll(); return false; }

        pcm_ = (int16_t*)malloc(sizeof(int16_t) * STEP_FRAMES * flac_->channels);
        if (!pcm_) { Serial.println("FLAC: out of memory"); closeAll(); return false; }
        Serial.printf("FLAC: %u Hz, %u ch, %u bit\n", (unsigned)flac_->sampleRate,
                      (unsigned)flac_->channels, (unsigned)flac_->bitsPerSample);
        return true;
    }

    DecodeStatus decode(PcmChunk& out) override {
        out.frames = 0;
        drflac_uint64 n = drflac_read_pcm_frames_s16(flac_, STEP_FRAMES, pcm_);
        if (n == 0) return DEC_END;
        out.data       = pcm_;
        out.frames     = (int)n;
        out.channels   = (int)flac_->channels;
        out.sampleRate = (int)flac_->sampleRate;
        return DEC_OK;
    }

    const char* name() const override { return "FLAC"; }

private:
    static const int STEP_FRAMES = 1024;

    void closeAll() {
        if (flac_) { drflac_close(flac_); flac_ = nullptr; }
        if (pcm_)  { free(pcm_); pcm_ = nullptr; }
        if (file_) file_.close();
    }

    File      file_;
    drflac*   flac_ = nullptr;
    int16_t*  pcm_  = nullptr;
};

}  // namespace

AudioDecoder* createFlacDecoder() { return new (std::nothrow) FlacDecoder(); }
