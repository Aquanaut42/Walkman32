// decoder_wav.cpp — RIFF/WAVE
// Supports integer PCM (8/16/24/32 bit) and 32-bit float, any channel count
// (only the first two channels are played) and any sample rate (resampled by the player).
// Skips unknown chunks (LIST, id3, ...), understands WAVE_FORMAT_EXTENSIBLE and
// streamed files whose data size is 0 / 0xFFFFFFFF.

#include "audio_decoder.h"
#include <new>
#include <stdlib.h>
#include <string.h>

namespace {

inline uint16_t le16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
inline uint32_t le32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

class WavDecoder : public AudioDecoder {
public:
    ~WavDecoder() override { closeAll(); }

    bool open(const char* path) override {
        file_ = SD.open(path);
        if (!file_) { Serial.printf("WAV: cannot open %s\n", path); return false; }
        if (!parseHeader()) { closeAll(); return false; }

        raw_ = (uint8_t*)malloc(RAW_SIZE);
        pcm_ = (int16_t*)malloc(sizeof(int16_t) * 2 * MAX_FRAMES);
        if (!raw_ || !pcm_) { Serial.println("WAV: out of memory"); closeAll(); return false; }
        return true;
    }

    DecodeStatus decode(PcmChunk& out) override {
        out.frames = 0;
        if (remaining_ < blockAlign_) return DEC_END;

        uint32_t want = RAW_SIZE - (RAW_SIZE % blockAlign_);
        if (want > MAX_FRAMES * blockAlign_) want = MAX_FRAMES * blockAlign_;
        if (want > remaining_) want = remaining_ - (remaining_ % blockAlign_);

        uint32_t got = (uint32_t)file_.read(raw_, want);
        if (got < want) remaining_ = 0;                 // file shorter than its header claims
        else            remaining_ -= got;
        int frames = (int)(got / blockAlign_);
        if (frames == 0) return DEC_END;

        const uint32_t outCh = channels_ < 2 ? 1 : 2;
        const uint8_t* p = raw_;
        int16_t* o = pcm_;
        for (int f = 0; f < frames; f++) {
            for (uint32_t c = 0; c < channels_; c++) {
                int16_t s = convert(p);
                p += bytesPerSample_;
                if (c < outCh) *o++ = s;
            }
        }
        out.data       = pcm_;
        out.frames     = frames;
        out.channels   = outCh;
        out.sampleRate = (int)sampleRate_;
        return DEC_OK;
    }

    const char* name() const override { return "WAV"; }

private:
    static const uint32_t RAW_SIZE   = 4096;
    static const int      MAX_FRAMES = 512;

    enum { FMT_PCM = 1, FMT_FLOAT = 3, FMT_EXTENSIBLE = 0xFFFE };

    int16_t convert(const uint8_t* p) const {
        switch (format_) {
            case FMT_FLOAT: {
                float v;
                memcpy(&v, p, 4);
                if (v > 1.0f) v = 1.0f;
                if (v < -1.0f) v = -1.0f;
                return (int16_t)(v * 32767.0f);
            }
            default:
                switch (bytesPerSample_) {
                    case 1:  return (int16_t)(((int)p[0] - 128) * 256);    // 8-bit WAV is unsigned
                    case 2:  return (int16_t)le16(p);
                    case 3:  return (int16_t)le16(p + 1);                  // keep the top 16 bits
                    default: return (int16_t)le16(p + 2);                  // 32-bit: top 16 bits
                }
        }
    }

    bool parseHeader() {
        uint8_t h[12];
        if (file_.read(h, 12) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) {
            Serial.println("WAV: not a RIFF/WAVE file");
            return false;
        }
        const uint32_t fileSize = (uint32_t)file_.size();
        bool haveFmt = false;
        uint32_t pos = 12;

        for (int guard = 0; guard < 64; guard++) {
            uint8_t ch[8];
            if (!file_.seek(pos) || file_.read(ch, 8) != 8) break;
            uint32_t size = le32(ch + 4);
            uint32_t body = pos + 8;

            if (!memcmp(ch, "fmt ", 4)) {
                uint8_t f[40];
                uint32_t n = size < sizeof(f) ? size : sizeof(f);
                if (n < 16 || file_.read(f, n) != n) break;
                format_         = le16(f);
                channels_       = le16(f + 2);
                sampleRate_     = le32(f + 4);
                blockAlign_     = le16(f + 12);
                uint32_t bits   = le16(f + 14);
                if (format_ == FMT_EXTENSIBLE && n >= 26) format_ = le16(f + 24);   // sub-format tag
                bytesPerSample_ = (bits + 7) / 8;
                haveFmt = true;
            } else if (!memcmp(ch, "data", 4)) {
                if (!haveFmt) break;
                if (!validFormat()) return false;
                remaining_ = size;
                if (size == 0 || size == 0xFFFFFFFFu || body + size > fileSize)
                    remaining_ = fileSize > body ? fileSize - body : 0;       // streamed / truncated
                file_.seek(body);
                return true;
            }
            uint32_t next = body + size + (size & 1);                          // chunks are word aligned
            if (next <= pos) break;
            pos = next;
            if (pos >= fileSize) break;
        }
        Serial.println("WAV: no usable fmt/data chunk");
        return false;
    }

    bool validFormat() {
        bool ok = false;
        if (format_ == FMT_PCM)
            ok = bytesPerSample_ >= 1 && bytesPerSample_ <= 4;
        else if (format_ == FMT_FLOAT)
            ok = bytesPerSample_ == 4;
        if (!ok || channels_ < 1 || channels_ > 8 || sampleRate_ < 4000 || sampleRate_ > 192000) {
            Serial.printf("WAV: unsupported format tag=%u bytes/sample=%u ch=%u rate=%u\n",
                          (unsigned)format_, (unsigned)bytesPerSample_,
                          (unsigned)channels_, (unsigned)sampleRate_);
            return false;
        }
        // Trust the real layout over a bogus blockAlign
        blockAlign_ = bytesPerSample_ * channels_;
        return true;
    }

    void closeAll() {
        if (raw_) { free(raw_); raw_ = nullptr; }
        if (pcm_) { free(pcm_); pcm_ = nullptr; }
        if (file_) file_.close();
    }

    File      file_;
    uint8_t*  raw_ = nullptr;
    int16_t*  pcm_ = nullptr;
    uint32_t  format_ = 0, channels_ = 0, sampleRate_ = 0, blockAlign_ = 4, bytesPerSample_ = 2;
    uint32_t  remaining_ = 0;
};

}  // namespace

AudioDecoder* createWavDecoder() { return new (std::nothrow) WavDecoder(); }
