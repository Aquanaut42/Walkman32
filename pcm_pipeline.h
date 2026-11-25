// pcm_pipeline.h
// Everything between "a decoder produced PCM" and "the A2DP callback consumes PCM".
//
//   decoder chunk (any rate, 1..n channels)
//        -> PcmResampler  (channel mapping + linear resample to 44.1 kHz stereo)
//        -> PcmRing       (read by the BT stack on Core 0)
//
// Header-only and free of Arduino/ESP32 dependencies so it can be tested on a PC.
#pragma once
#include <stdint.h>
#include <stddef.h>

// A block of decoded audio. 'data' is owned by the decoder and stays valid
// until that decoder's next decode() call.
struct PcmChunk {
    const int16_t* data;     // interleaved samples
    int frames;              // samples per channel
    int channels;            // 1 = mono, 2 = stereo, >2 = only the first two are used
    int sampleRate;          // Hz
};

// ── Ring buffer of stereo frames (one writer, one reader) ────────────────────
// A frame is stored as ONE 32-bit word (low 16 bits = left, high 16 bits = right). Everything is read and
// written in whole words, so the storage may live in memory that only allows 32-bit access, such as the
// ESP32's spare instruction RAM - which keeps 16 KB of the normal heap free (see playback.cpp).
class PcmRing {
    uint32_t*    buf_  = nullptr;
    int          cap_  = 0;          // capacity in frames; one slot always stays empty
    volatile int head_ = 0;          // frame index, written by the decoder (Core 1)
    volatile int tail_ = 0;          // frame index, written by the BT callback (Core 0)
    int used() const { int h = head_, t = tail_; return h >= t ? h - t : cap_ - t + h; }
public:
    void begin(uint32_t* storage, int frames) { buf_ = storage; cap_ = frames; head_ = tail_ = 0; }
    bool ready() const { return buf_ != nullptr; }

    int  available() const { return buf_ ? used() * 2 : 0; }          // in samples (2 per frame)
    int  freeFrames() const { return buf_ ? cap_ - used() - 1 : 0; }
    void clear() { head_ = tail_ = 0; }

    void writeFrames(const int16_t* stereo, int frames) {
        int h = head_;
        for (int i = 0; i < frames; i++) {
            buf_[h] = (uint32_t)(uint16_t)stereo[i * 2] | ((uint32_t)(uint16_t)stereo[i * 2 + 1] << 16);
            if (++h == cap_) h = 0;
            head_ = h;               // publish only after the whole word is written
        }
    }
    bool readFrame(int16_t& l, int16_t& r) {
        if (!buf_ || head_ == tail_) return false;
        int t = tail_;
        uint32_t w = buf_[t];
        l = (int16_t)(w & 0xFFFF);
        r = (int16_t)(w >> 16);
        if (++t == cap_) t = 0;
        tail_ = t;
        return true;
    }
};

// ── Channel mapping + linear-interpolation resampler ─────────────────────────
// Output is always interleaved stereo. Mono is duplicated, >2 channels keep the
// first two. When the input rate already equals the output rate it is a plain copy.
class PcmResampler {
    static const uint32_t ONE = 1u << 16;     // 1.0 in Q16
    uint32_t step_  = ONE;                    // input frames advanced per output frame
    uint32_t phase_ = 0;                      // position between last_ and the next input frame
    int32_t  lastL_ = 0, lastR_ = 0;          // previous input frame
    bool     pass_  = true;
public:
    void configure(int inRate, int outRate) {
        pass_  = (inRate == outRate);
        step_  = (uint32_t)(((uint64_t)inRate << 16) / (uint32_t)outRate);
        phase_ = 0;
        lastL_ = lastR_ = 0;
    }

    // Converts up to 'maxOut' output frames. 'used' receives how many INPUT frames
    // were consumed; returns the number of OUTPUT frames written to 'out'.
    // Always makes progress when inFrames > 0 and maxOut > 0.
    int process(const int16_t* in, int inFrames, int ch,
                int16_t* out, int maxOut, int& used) {
        int i = 0, o = 0;                     // i: input frames, o: output samples

        if (pass_) {
            int n = inFrames < maxOut ? inFrames : maxOut;
            for (; i < n; i++) {
                int16_t l = in[i * ch];
                out[o++] = l;
                out[o++] = ch > 1 ? in[i * ch + 1] : l;
            }
            used = n;
            return n;
        }

        while (o < 2 * maxOut) {
            while (phase_ >= ONE) {           // step past input frames we have moved beyond
                if (i >= inFrames) { used = i; return o / 2; }
                phase_ -= ONE;
                lastL_ = in[i * ch];
                lastR_ = ch > 1 ? in[i * ch + 1] : lastL_;
                i++;
            }
            if (i >= inFrames) break;
            int32_t curL = in[i * ch];
            int32_t curR = ch > 1 ? in[i * ch + 1] : curL;
            int32_t f    = (int32_t)(phase_ >> 4);                  // 12-bit fraction
            out[o++] = (int16_t)(lastL_ + (((curL - lastL_) * f) >> 12));
            out[o++] = (int16_t)(lastR_ + (((curR - lastR_) * f) >> 12));
            phase_ += step_;
        }
        used = i;
        return o / 2;
    }
};

// ── Holds one decoded chunk and moves it into the ring as space allows ───────
template <class Ring>
class PcmFeeder {
public:
    static const int OUT_RATE = 44100;        // A2DP source is fixed at 44.1 kHz stereo

    explicit PcmFeeder(Ring& ring) : ring_(ring) { reset(); }

    void reset() { chunk_ = PcmChunk{nullptr, 0, 0, 0}; pos_ = 0; rate_ = 0; }
    void stage(const PcmChunk& c) { chunk_ = c; pos_ = 0; }

    // Returns true when the staged chunk has been moved completely (or there is none).
    // Returns false when the ring is full; call again later.
    bool drain() {
        if (chunk_.frames <= 0 || pos_ >= chunk_.frames) return true;
        if (chunk_.data == nullptr || chunk_.channels < 1 ||
            chunk_.sampleRate < 4000 || chunk_.sampleRate > 192000) {
            pos_ = chunk_.frames;             // unusable chunk — drop it
            return true;
        }
        if (chunk_.sampleRate != rate_) {
            rs_.configure(chunk_.sampleRate, OUT_RATE);
            rate_ = chunk_.sampleRate;
        }
        int16_t tmp[2 * 256];
        while (pos_ < chunk_.frames) {
            int room = ring_.freeFrames();
            if (room <= 0) return false;
            if (room > 256) room = 256;
            int used = 0;
            int made = rs_.process(chunk_.data + (size_t)pos_ * chunk_.channels,
                                   chunk_.frames - pos_, chunk_.channels, tmp, room, used);
            if (made > 0) ring_.writeFrames(tmp, made);
            pos_ += used;
            if (made == 0 && used == 0) pos_ = chunk_.frames;   // defensive: never stall
        }
        return true;
    }

private:
    Ring&        ring_;
    PcmResampler rs_;
    PcmChunk     chunk_;
    int          pos_;
    int          rate_;
};
