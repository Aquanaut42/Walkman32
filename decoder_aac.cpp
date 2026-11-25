// decoder_aac.cpp — AAC via Helix AAC
//   .aac  raw ADTS stream  → AACFindSyncWord + AACDecode
//   .m4a  MP4 container    → Mp4Track hands out raw AAC frames, Helix runs in "raw block" mode
//
// Supported: AAC-LC (and AAC Main), mono or stereo, 8–96 kHz.
// Not supported: ALAC ("Apple Lossless" .m4a), >2 channels, fragmented MP4, DRM-protected files.
// HE-AAC plays too, but the SBR enhancement only runs if Helix was built with SBR enabled
// (HELIX_FEATURE_AUDIO_CODEC_AAC_SBR); otherwise it is decoded at the core sample rate.
//
// libhelix's allocator does `while(true);` if a malloc fails, which freezes the whole ESP32. To make that
// impossible the decoder state is allocated here with a malloc that is allowed to fail and handed to
// Helix through AACInitDecoderPre(); Helix never allocates anything itself.
//
// Memory while an AAC track plays (measured, 32-bit): about 40 KB, or about 95 KB when Helix is built
// with SBR. libhelix turns SBR on by default (ConfigHelix.h) - comment that #define out there to
// save ~58 KB if your free heap is tight and you don't need HE-AAC.

#include "audio_decoder.h"

#if !FW_ENABLE_AAC

AudioDecoder* createAacDecoder(bool) {
    Serial.println("AAC/M4A: support not compiled in (Helix AAC decoder 'aacdec.h' not found - see config.h)");
    return nullptr;
}

#else

#include FW_AAC_HEADER
#if __has_include("ConfigHelix.h")
  #include "ConfigHelix.h"
  #ifndef HELIX_FEATURE_AUDIO_CODEC_AAC_SBR
    #define FW_AAC_NO_SBR 1          // Helix was built without HE-AAC: half the output buffer, ~58 KB less heap
  #endif
#endif
#include "mp4_track.h"
#include <new>
#include <stdlib.h>
#include <string.h>

namespace {

// ── AudioSpecificConfig (ISO 14496-3) ────────────────────────────────────────
const int kAacRates[13] = {96000, 88200, 64000, 48000, 44100, 32000, 24000,
                           22050, 16000, 12000, 11025, 8000, 7350};
const uint8_t kAacChannels[8] = {0, 1, 2, 3, 4, 5, 6, 8};

struct AscBits {
    const uint8_t* p; int len; int pos;
    uint32_t get(int n) {
        uint32_t v = 0;
        while (n-- > 0) {
            int byte = pos >> 3;
            int bit  = byte < len ? (p[byte] >> (7 - (pos & 7))) & 1 : 0;
            v = (v << 1) | (uint32_t)bit;
            pos++;
        }
        return v;
    }
};

// Returns false if the config describes something Helix cannot decode.
bool parseAsc(const uint8_t* a, int len, int& profile, int& coreRate, int& channels) {
    AscBits b = {a, len, 0};
    int aot = (int)b.get(5);
    if (aot == 31) aot = 32 + (int)b.get(6);
    int idx = (int)b.get(4);
    if (idx == 15) coreRate = (int)b.get(24);
    else if (idx < 13) coreRate = kAacRates[idx];
    else return false;
    int chCfg = (int)b.get(4);
    if (chCfg > 0 && chCfg < 8) channels = kAacChannels[chCfg];   // 0 = in-band PCE: keep stsd's value

    if (aot == 5 || aot == 29) {                                   // HE-AAC / HE-AACv2: core codec follows
        int ext = (int)b.get(4);
        if (ext == 15) b.get(24);
        aot = (int)b.get(5);
        if (aot == 31) aot = 32 + (int)b.get(6);
    }
    switch (aot) {
        case 1:  profile = AAC_PROFILE_MP; return true;
        case 2:  profile = AAC_PROFILE_LC; return true;
        case 3:  profile = AAC_PROFILE_SSR; return true;
        default: return false;                                     // LTP, ER-AAC, ...
    }
}

// ─────────────────────────────────────────────────────────────────────────────
class AacDecoder : public AudioDecoder {
public:
    explicit AacDecoder(bool mp4) : mp4_(mp4) {}
    ~AacDecoder() override { closeAll(); }

    bool open(const char* path) override {
        // Helix needs by far the biggest contiguous block, so it goes first.
        state_ = (uint8_t*)malloc(STATE_BYTES);
        if (state_) {
            memset(state_, 0, STATE_BYTES);
            h_ = AACInitDecoderPre(state_, STATE_BYTES);
        }
        pcm_   = (int16_t*)malloc(sizeof(int16_t) * PCM_SAMPLES);
        inBuf_ = (uint8_t*)malloc((mp4_ ? MP4_FRAME_MAX : ADTS_IN_SIZE) + 8);
        if (!state_ || !h_ || !pcm_ || !inBuf_) {
            Serial.printf("AAC: out of memory (free heap %u, largest free block %u bytes)\n",
                          (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
#ifndef FW_AAC_NO_SBR
            Serial.println("AAC: Helix is built with SBR/HE-AAC (needs ~50 KB more). To save it, comment out the "
                           "HELIX_FEATURE_AUDIO_CODEC_AAC_SBR define in the libhelix library's src/ConfigHelix.h");
#endif
            closeAll();
            return false;
        }

        return mp4_ ? openMp4(path) : openAdts(path);
    }

    DecodeStatus decode(PcmChunk& out) override {
        out.frames = 0;
        return mp4_ ? decodeMp4(out) : decodeAdts(out);
    }

    const char* name() const override { return mp4_ ? "AAC (M4A)" : "AAC (ADTS)"; }

private:
#ifdef FW_AAC_NO_SBR
    static const int PCM_SAMPLES   = AAC_MAX_NCHANS * AAC_MAX_NSAMPS;       // no SBR: 1024 frames max
#else
    static const int PCM_SAMPLES   = AAC_MAX_NCHANS * AAC_MAX_NSAMPS * 2;   // SBR doubles the output
#endif
    // Decoder state handed to AACInitDecoderPre(): AACDecInfo (96) + PSInfoBase (20560), measured on the
    // real library; with SBR the PSInfoBase grows to 28752 and a 50788-byte SBR state is added.
#ifdef FW_AAC_NO_SBR
    static const int STATE_BYTES   = 96 + 20560 + 64;
#else
    static const int STATE_BYTES   = 96 + 28752 + 50788 + 64;
#endif
    static const int ADTS_IN_SIZE  = 4096;
    static const int MP4_FRAME_MAX = 3072;     // largest AAC frame we accept (2 channels need < 1600)

    // ── .aac ─────────────────────────────────────────────────────────────────
    bool openAdts(const char* path) {
        file_ = SD.open(path);
        if (!file_) { Serial.printf("AAC: cannot open %s\n", path); closeAll(); return false; }
        uint32_t skip = audioId3v2Size(file_);
        if (skip) file_.seek(skip);
        inFill_ = inPos_ = 0;
        return true;
    }

    DecodeStatus decodeAdts(PcmChunk& out) {
        refillInput();
        if (inFill_ - inPos_ < 4) return file_.available() ? DEC_OK : DEC_END;

        int offset = AACFindSyncWord(inBuf_ + inPos_, inFill_ - inPos_);
        if (offset < 0) { inPos_ = inFill_; return DEC_OK; }
        inPos_ += offset;

        unsigned char* ptr = inBuf_ + inPos_;
        int bytesLeft = inFill_ - inPos_;
        int err = AACDecode(h_, &ptr, &bytesLeft, pcm_);
        if (err != ERR_AAC_NONE) { noteError(err); inPos_++; return DEC_OK; }
        inPos_ = inFill_ - bytesLeft;
        nOk_++;
        return publish(out);
    }

    void refillInput() {
        if (inPos_ > 0) {
            memmove(inBuf_, inBuf_ + inPos_, inFill_ - inPos_);
            inFill_ -= inPos_;
            inPos_ = 0;
        }
        int space = ADTS_IN_SIZE - inFill_;
        if (space > 0 && file_.available()) {
            int got = file_.read(inBuf_ + inFill_, space);
            if (got > 0) inFill_ += got;
        }
    }

    // ── .m4a ─────────────────────────────────────────────────────────────────
    bool openMp4(const char* path) {
        trk_ = new (std::nothrow) Mp4Track();
        if (!trk_ || !trk_->open(path)) { Serial.println("M4A: cannot parse container"); closeAll(); return false; }

        bool aacCodec = trk_->codec() == (((uint32_t)'m' << 24) | ((uint32_t)'p' << 16) | ((uint32_t)'4' << 8) | 'a') &&
                        (trk_->objectType() == 0 || trk_->objectType() == 0x40 ||
                         (trk_->objectType() >= 0x66 && trk_->objectType() <= 0x68));
        if (!aacCodec) {
            Serial.println("M4A: codec is not AAC (Apple Lossless / other formats are not supported)");
            closeAll();
            return false;
        }

        int profile = AAC_PROFILE_LC;                         // assume LC when no config is present
        int rate = trk_->sampleRate();
        int ch   = trk_->channels();
        if (trk_->config() && !parseAsc(trk_->config(), trk_->configLen(), profile, rate, ch)) {
            Serial.println("M4A: unsupported AAC object type");
            closeAll();
            return false;
        }
        if (ch < 1 || ch > AAC_MAX_NCHANS || rate < 8000) {
            Serial.printf("M4A: unsupported layout (%d ch, %d Hz)\n", ch, rate);
            closeAll();
            return false;
        }

        AACFrameInfo fi;
        memset(&fi, 0, sizeof(fi));
        fi.nChans       = ch;
        fi.sampRateCore = rate;
        fi.profile      = profile;
        if (AACSetRawBlockParams(h_, 0, &fi) != 0) {
            Serial.println("M4A: decoder rejected stream parameters");
            closeAll();
            return false;
        }

        file_ = SD.open(path);                                // second handle: audio data only
        if (!file_) { closeAll(); return false; }
        return true;
    }

    DecodeStatus decodeMp4(PcmChunk& out) {
        uint32_t off, size;
        if (!trk_->nextSample(off, size)) return DEC_END;
        if (size == 0 || size > MP4_FRAME_MAX) return DEC_OK;            // skip bogus frame
        if (file_.position() != off && !file_.seek(off)) return DEC_ERROR;
        if ((uint32_t)file_.read(inBuf_, size) != size) return DEC_END;  // truncated file
        memset(inBuf_ + size, 0, 8);                                     // padding after the frame

        unsigned char* ptr = inBuf_;
        int bytesLeft = (int)size;
        int err = AACDecode(h_, &ptr, &bytesLeft, pcm_);
        if (err != ERR_AAC_NONE) { noteError(err); return DEC_OK; }                 // drop damaged frame
        nOk_++;
        return publish(out);
    }

    // ── shared ───────────────────────────────────────────────────────────────
    // A frame Helix cannot decode is skipped; report the first few so problems are visible.
    void noteError(int err) {
        if (nErr_++ < 8)
            Serial.printf("AAC: frame decode error %d after %u good frames\n",
                          err, (unsigned)nOk_);
    }

    DecodeStatus publish(PcmChunk& out) {
        AACFrameInfo info;
        AACGetLastFrameInfo(h_, &info);
        if (info.nChans < 1 || info.sampRateOut < 1) return DEC_OK;
        out.data       = pcm_;
        out.channels   = info.nChans;
        out.sampleRate = info.sampRateOut;
        out.frames     = info.outputSamps / info.nChans;
        return DEC_OK;
    }

    void closeAll() {
        h_ = nullptr;                                         // state lives inside state_, no AACFreeDecoder()
        if (state_) { free(state_); state_ = nullptr; }
        if (trk_)   { trk_->close(); delete trk_; trk_ = nullptr; }
        if (pcm_)   { free(pcm_);   pcm_   = nullptr; }
        if (inBuf_) { free(inBuf_); inBuf_ = nullptr; }
        if (file_)  file_.close();
    }

    bool         mp4_;
    File         file_;
    HAACDecoder  h_     = nullptr;
    uint8_t*     state_ = nullptr;
    int16_t*     pcm_   = nullptr;
    uint8_t*     inBuf_ = nullptr;
    int          inFill_ = 0, inPos_ = 0;
    Mp4Track*    trk_   = nullptr;
    uint32_t     nOk_ = 0, nErr_ = 0;
};

}  // namespace

AudioDecoder* createAacDecoder(bool mp4Container) { return new (std::nothrow) AacDecoder(mp4Container); }

#endif  // FW_ENABLE_AAC
