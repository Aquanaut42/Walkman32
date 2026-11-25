// mp4_track.h — minimal MP4/M4A demuxer (first audio track only)
//
// Finds the audio track in an MP4 container and hands out its samples (AAC frames) one by one
// as (file offset, size). The sample tables (stsz/stsc/stco) can have thousands of entries, so they are
// NOT loaded into RAM: they are read lazily through a small window using a second file handle.
// Total RAM use is about 1.5 KB. Fragmented MP4 (moof) and files > 4 GB are not supported.
#pragma once
#include <Arduino.h>
#include <SD.h>

class Mp4Track {
public:
    bool open(const char* path);
    void close();

    // Codec as declared in the sample description
    uint32_t       codec() const        { return codec_; }        // 'mp4a', 'alac', ...
    uint8_t        objectType() const   { return oti_; }          // 0x40 = MPEG-4 AAC (0 if unknown)
    const uint8_t* config() const       { return ascLen_ ? asc_ : nullptr; }   // AudioSpecificConfig
    int            configLen() const    { return ascLen_; }
    int            channels() const     { return channels_; }
    int            sampleRate() const   { return sampleRate_; }
    uint32_t       sampleCount() const  { return sampleCount_; }

    // Next sample in playback order. Returns false at the end of the track.
    bool nextSample(uint32_t& offset, uint32_t& size);

private:
    // Sliding window over one on-disk table of fixed-size entries
    struct Table {
        uint32_t base = 0;           // file offset of entry 0
        uint32_t count = 0;
        uint32_t esz = 4;            // bytes per entry
        uint32_t winFirst = 0, winCount = 0;
        uint8_t* win = nullptr;      // window storage
        uint32_t winCap = 0;         // window capacity in entries
    };
    const uint8_t* entry(Table& t, uint32_t i);

    bool parseTrack(uint32_t start, uint32_t end);
    bool parseSampleDescription(uint32_t start, uint32_t end);
    void parseEsds(uint32_t start, uint32_t end);
    bool setStscEntry(uint32_t idx);
    bool beginChunk(uint32_t chunk);

    File     tbl_;                   // handle used for atoms and tables (the player has its own for audio data)
    uint32_t codec_ = 0;
    uint8_t  oti_ = 0;
    uint8_t  asc_[8];
    int      ascLen_ = 0;
    int      channels_ = 0, sampleRate_ = 0;

    uint32_t sampleCount_ = 0, fixedSize_ = 0;
    Table    stsz_, stsc_, stco_;
    bool     co64_ = false;
    uint8_t  szWin_[512];            // window of 128 stsz entries
    uint8_t  scWin_[384];            // window of  32 stsc entries
    uint8_t  coWin_[512];            // window of 128 stco entries (64 if co64)

    // Iteration state
    uint32_t sampleIdx_ = 0;
    uint32_t nextChunk_ = 0;         // 0-based chunk to start next
    uint32_t chunkLeft_ = 0;         // samples remaining in the current chunk
    uint32_t nextOff_   = 0;         // file offset of the next sample
    uint32_t scIdx_ = 0, scSpc_ = 0, scNextFirst_ = 0;
};
