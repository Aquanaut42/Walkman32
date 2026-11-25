// decoder_mp3.cpp — Helix MP3
// Same decode logic as the original playback.cpp, wrapped in the AudioDecoder interface.
// Channel count and sample rate are now reported to the player, so mono files and
// 22.05/32/48 kHz files play at the right speed.
//
// Hardening (Helix itself is not robust against junk that looks like a frame header):
//   - the ID3v2 tag (often tens of KB of cover art) is skipped instead of being scanned for sync words
//   - only Layer III headers are handed to Helix (it also "accepts" Layer I/II and then misdecodes them)
//   - if Helix fails its free-format bitrate probe it stays in a broken state (nSlots = -1, later
//     memcpy(-1) crash), so the decoder is re-created after that error
//   - libhelix's allocator does `while(true);` when malloc fails, which hangs the whole ESP32.
//     So before calling MP3InitDecoder() the allocations it is going to make are rehearsed with a
//     malloc that is allowed to fail, and the track is refused cleanly if they do not fit.

#include "audio_decoder.h"
#include "mp3dec.h"
#include <new>
#include <stdlib.h>

namespace {

class Mp3Decoder : public AudioDecoder {
public:
    ~Mp3Decoder() override { closeAll(); }

    bool open(const char* path) override {
        file_ = SD.open(path);
        if (!file_) { Serial.printf("MP3: cannot open %s\n", path); return false; }

        // Biggest consumer first (least fragmentation), and only if it is certain to fit.
        if (!helixWillFit()) {
            Serial.printf("MP3: not enough free memory (free heap %u, largest free block %u bytes)\n",
                          (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
            closeAll();
            return false;
        }
        h_     = MP3InitDecoder();
        inBuf_ = (uint8_t*)malloc(IN_SIZE);
        pcm_   = (int16_t*)malloc(sizeof(int16_t) * MAX_NCHAN * MAX_NGRAN * MAX_NSAMP);
        if (!inBuf_ || !pcm_ || !h_) {
            Serial.printf("MP3: out of memory (free heap %u, largest free block %u bytes)\n",
                          (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
            closeAll();
            return false;
        }
        inFill_ = inPos_ = 0;
        uint32_t skip = audioId3v2Size(file_);
        if (skip) file_.seek(skip);
        return true;
    }

    DecodeStatus decode(PcmChunk& out) override {
        out.frames = 0;
        refillInput();
        if (inFill_ - inPos_ < 4) {
            return file_.available() ? DEC_OK : DEC_END;
        }
        int offset = MP3FindSyncWord(inBuf_ + inPos_, inFill_ - inPos_);
        if (offset < 0) { inPos_ = inFill_; return DEC_OK; }
        inPos_ += offset;

        if (!plausibleHeader(inBuf_ + inPos_)) { inPos_++; return DEC_OK; }

        uint8_t* ptr = inBuf_ + inPos_;
        int bytesLeft = inFill_ - inPos_;
        int err = MP3Decode(h_, &ptr, &bytesLeft, pcm_, 0);
        if (err != ERR_MP3_NONE) {
            if (err == ERR_MP3_FREE_BITRATE_SYNC) {          // Helix is now in a corrupt state: start fresh
                MP3FreeDecoder(h_);
                h_ = MP3InitDecoder();
                if (!h_) return DEC_ERROR;
            }
            inPos_++;
            return DEC_OK;
        }
        inPos_ = inFill_ - bytesLeft;

        MP3FrameInfo info;
        MP3GetLastFrameInfo(h_, &info);
        if (info.nChans < 1) return DEC_OK;
        out.data       = pcm_;
        out.channels   = info.nChans;
        out.sampleRate = info.samprate;
        out.frames     = info.outputSamps / info.nChans;
        return DEC_OK;
    }

    const char* name() const override { return "MP3"; }

private:
    static const int IN_SIZE = 4096;

    // The exact sizes MP3InitDecoder() requests, in order (measured: DecInfo, FrameHeader, SideInfo,
    // ScaleFactorInfo, HuffmanInfo, DequantInfo, IMDCTInfo, SubbandInfo - 23.8 KB in total).
    // If a newer libhelix asked for more, this rehearsal would be optimistic, never pessimistic-wrong.
    static bool helixWillFit() {
        static const int kSizes[] = {2032, 56, 328, 284, 4624, 840, 6944, 8708};
        void* p[sizeof(kSizes) / sizeof(kSizes[0])];
        int n = 0;
        bool ok = true;
        for (; n < (int)(sizeof(kSizes) / sizeof(kSizes[0])); n++) {
            p[n] = malloc(kSizes[n] + 16);                 // +16: allocator bookkeeping / rounding
            if (!p[n]) { ok = false; break; }
        }
        while (n > 0) free(p[--n]);
        return ok;
    }

    // MPEG audio header: version not reserved, Layer III, bitrate/sample-rate index valid.
    static bool plausibleHeader(const uint8_t* h) {
        return ((h[1] >> 3) & 3) != 1 &&        // version bits 01 = reserved
               ((h[1] >> 1) & 3) == 1 &&        // layer bits 01 = Layer III
               (h[2] >> 4) != 15 &&             // bitrate index 15 = invalid
               ((h[2] >> 2) & 3) != 3;          // sample-rate index 3 = reserved
    }

    void refillInput() {
        if (inPos_ > 0) {
            memmove(inBuf_, inBuf_ + inPos_, inFill_ - inPos_);
            inFill_ -= inPos_;
            inPos_ = 0;
        }
        int space = IN_SIZE - inFill_;
        if (space > 0 && file_.available()) {
            int got = file_.read(inBuf_ + inFill_, space);
            if (got > 0) inFill_ += got;
        }
    }

    void closeAll() {
        if (h_)     { MP3FreeDecoder(h_); h_ = nullptr; }
        if (inBuf_) { free(inBuf_); inBuf_ = nullptr; }
        if (pcm_)   { free(pcm_);   pcm_   = nullptr; }
        if (file_)  file_.close();
    }

    File        file_;
    HMP3Decoder h_     = nullptr;
    uint8_t*    inBuf_ = nullptr;
    int16_t*    pcm_   = nullptr;
    int         inFill_ = 0, inPos_ = 0;
};

}  // namespace

AudioDecoder* createMp3Decoder() { return new (std::nothrow) Mp3Decoder(); }
