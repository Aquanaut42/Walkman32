// audio_decoder.h
// One small interface for every file format the player understands.
// playback.cpp only talks to AudioDecoder; each format lives in its own decoder_*.cpp.
//
//   MP3   decoder_mp3.cpp    Helix MP3
//   WAV   decoder_wav.cpp    PCM 8/16/24/32-bit and 32-bit float (no library needed)
//   FLAC  decoder_flac.cpp   dr_flac (single header, bundled as dr_flac.h)
//   AAC   decoder_aac.cpp    Helix AAC: .aac (ADTS) and .m4a (MP4 container, AAC-LC)
#pragma once
#include <Arduino.h>
#include <SD.h>
#include "config.h"
#include "pcm_pipeline.h"

// AAC/M4A needs the Helix AAC decoder, e.g. from the "libhelix" Arduino library. It is switched on
// automatically when its header is on the include path. To force it on/off (or to point at a
// differently laid out copy of Helix) set FW_ENABLE_AAC / FW_AAC_HEADER in config.h.
#ifndef FW_ENABLE_AAC
  #ifdef __has_include
    #if __has_include("libhelix-aac/aacdec.h") || __has_include("aacdec.h")
      #define FW_ENABLE_AAC 1
    #endif
  #endif
  #ifndef FW_ENABLE_AAC
    #define FW_ENABLE_AAC 0
  #endif
#endif
#ifndef FW_AAC_HEADER
  #ifdef __has_include
    #if !__has_include("libhelix-aac/aacdec.h") && __has_include("aacdec.h")
      #define FW_AAC_HEADER "aacdec.h"
    #endif
  #endif
  #ifndef FW_AAC_HEADER
    #define FW_AAC_HEADER "libhelix-aac/aacdec.h"
  #endif
#endif

enum DecodeStatus {
    DEC_OK,       // call decode() again (the chunk may be empty)
    DEC_END,      // end of track
    DEC_ERROR     // unrecoverable error — the player treats it like end of track
};

class AudioDecoder {
public:
    virtual ~AudioDecoder() {}
    virtual bool open(const char* path) = 0;             // false = cannot play this file
    // Decode the next block. Every call must consume input, so it can never spin forever.
    virtual DecodeStatus decode(PcmChunk& out) = 0;
    virtual const char* name() const = 0;
};

enum AudioFormat {
    FORMAT_UNKNOWN,
    FORMAT_MP3,
    FORMAT_WAV,
    FORMAT_FLAC,
    FORMAT_AAC,    // raw ADTS stream (.aac)
    FORMAT_M4A     // MP4 container (.m4a)
};

AudioFormat   audioFormatFromPath(const char* path);
// Returns nullptr when the format is unknown or not compiled in.
AudioDecoder* audioDecoderCreate(AudioFormat format);

// Size in bytes of an ID3v2 tag at the start of 'f' (0 if none). Leaves the file at position 0.
uint32_t audioId3v2Size(File& f);

// Per-format factories (implemented in decoder_*.cpp)
AudioDecoder* createMp3Decoder();
AudioDecoder* createWavDecoder();
AudioDecoder* createFlacDecoder();
AudioDecoder* createAacDecoder(bool mp4Container);
