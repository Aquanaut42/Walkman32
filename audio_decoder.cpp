// audio_decoder.cpp — picks the right decoder for a file

#include "audio_decoder.h"
#include <string.h>

AudioFormat audioFormatFromPath(const char* path) {
    if (!path) return FORMAT_UNKNOWN;
    const char* dot   = strrchr(path, '.');
    const char* slash = strrchr(path, '/');
    if (!dot || (slash && dot < slash)) return FORMAT_UNKNOWN;

    char ext[8];
    size_t n = strlen(dot);
    if (n >= sizeof(ext)) return FORMAT_UNKNOWN;
    for (size_t i = 0; i <= n; i++) {
        char c = dot[i];
        ext[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
    }

    if (!strcmp(ext, ".mp3"))  return FORMAT_MP3;
    if (!strcmp(ext, ".wav"))  return FORMAT_WAV;
    if (!strcmp(ext, ".flac")) return FORMAT_FLAC;
    if (!strcmp(ext, ".aac"))  return FORMAT_AAC;
    if (!strcmp(ext, ".m4a"))  return FORMAT_M4A;
    return FORMAT_UNKNOWN;
}

AudioDecoder* audioDecoderCreate(AudioFormat format) {
    switch (format) {
        case FORMAT_MP3:  return createMp3Decoder();
        case FORMAT_WAV:  return createWavDecoder();
        case FORMAT_FLAC: return createFlacDecoder();
        case FORMAT_AAC:  return createAacDecoder(false);
        case FORMAT_M4A:  return createAacDecoder(true);
        default:          return nullptr;
    }
}

uint32_t audioId3v2Size(File& f) {
    uint8_t h[10];
    f.seek(0);
    if (f.read(h, 10) != 10 || h[0] != 'I' || h[1] != 'D' || h[2] != '3') {
        f.seek(0);
        return 0;
    }
    uint32_t size = ((uint32_t)(h[6] & 0x7F) << 21) | ((uint32_t)(h[7] & 0x7F) << 14) |
                    ((uint32_t)(h[8] & 0x7F) << 7)  |  (uint32_t)(h[9] & 0x7F);
    size += 10;
    if (h[5] & 0x10) size += 10;               // footer present
    f.seek(0);
    return size < (uint32_t)f.size() ? size : 0;
}
