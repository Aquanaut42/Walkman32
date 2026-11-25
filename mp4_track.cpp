// mp4_track.cpp — see mp4_track.h

#include "mp4_track.h"
#include <string.h>

#define FOURCC(a, b, c, d) (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | ((uint32_t)(c) << 8) | (uint32_t)(d))

namespace {

inline uint32_t be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}
inline uint16_t be16(const uint8_t* p) { return (uint16_t)((p[0] << 8) | p[1]); }

struct Box {
    uint32_t type;
    uint32_t start;    // first payload byte
    uint32_t end;      // one past the last byte of the box
};

// Read the next box header inside [pos, to). Advances 'pos' to the following sibling.
bool nextBox(File& f, uint32_t& pos, uint32_t to, Box& b) {
    if (pos + 8 > to) return false;
    uint8_t h[16];
    if (!f.seek(pos) || f.read(h, 8) != 8) return false;
    uint64_t size = be32(h);
    uint32_t hdr = 8;
    if (size == 1) {                                   // 64-bit size follows
        if (f.read(h + 8, 8) != 8) return false;
        size = ((uint64_t)be32(h + 8) << 32) | be32(h + 12);
        hdr = 16;
    } else if (size == 0) {
        size = to - pos;                               // extends to the end
    }
    if (size < hdr) return false;
    uint64_t endp = (uint64_t)pos + size;
    if (endp > to) endp = to;                          // tolerate truncated files
    b.type  = be32(h + 4);
    b.start = pos + hdr;
    b.end   = (uint32_t)endp;
    pos     = (uint32_t)endp;
    return true;
}

bool findBox(File& f, uint32_t from, uint32_t to, uint32_t type, Box& out) {
    uint32_t pos = from;
    Box b;
    for (int guard = 0; guard < 512 && nextBox(f, pos, to, b); guard++) {
        if (b.type == type) { out = b; return true; }
    }
    return false;
}

bool readAt(File& f, uint32_t pos, uint8_t* buf, uint32_t n) {
    return f.seek(pos) && f.read(buf, n) == n;
}

}  // namespace

// ─────────────────────────────────────────────────────────────────────────────
bool Mp4Track::open(const char* path) {
    close();
    tbl_ = SD.open(path);
    if (!tbl_) return false;

    const uint32_t fileEnd = (uint32_t)tbl_.size();
    Box moov;
    if (!findBox(tbl_, 0, fileEnd, FOURCC('m', 'o', 'o', 'v'), moov)) {
        Serial.println("M4A: no moov box (fragmented MP4 is not supported)");
        close();
        return false;
    }

    // First 'trak' that is an audio track with complete sample tables
    uint32_t pos = moov.start;
    Box b;
    bool found = false;
    for (int guard = 0; guard < 64 && nextBox(tbl_, pos, moov.end, b); guard++) {
        if (b.type == FOURCC('t', 'r', 'a', 'k') && parseTrack(b.start, b.end)) { found = true; break; }
    }
    if (!found || sampleCount_ == 0) {
        Serial.println("M4A: no usable audio track");
        close();
        return false;
    }

    // Prepare iteration
    sampleIdx_ = nextChunk_ = chunkLeft_ = nextOff_ = 0;
    scIdx_ = 0;
    if (!setStscEntry(0)) { close(); return false; }
    return true;
}

void Mp4Track::close() {
    if (tbl_) tbl_.close();
    codec_ = 0; oti_ = 0; ascLen_ = 0; channels_ = sampleRate_ = 0;
    sampleCount_ = fixedSize_ = 0;
    stsz_ = Table(); stsc_ = Table(); stco_ = Table();
    co64_ = false;
}

// ── Track / table parsing ────────────────────────────────────────────────────
bool Mp4Track::parseTrack(uint32_t start, uint32_t end) {
    Box mdia, hdlr, minf, stbl, stsd, stsz, stsc, stco;
    if (!findBox(tbl_, start, end, FOURCC('m', 'd', 'i', 'a'), mdia)) return false;
    if (!findBox(tbl_, mdia.start, mdia.end, FOURCC('h', 'd', 'l', 'r'), hdlr)) return false;

    uint8_t t[12];
    if (hdlr.end - hdlr.start < 12 || !readAt(tbl_, hdlr.start, t, 12)) return false;
    if (be32(t + 8) != FOURCC('s', 'o', 'u', 'n')) return false;          // not an audio track

    if (!findBox(tbl_, mdia.start, mdia.end, FOURCC('m', 'i', 'n', 'f'), minf)) return false;
    if (!findBox(tbl_, minf.start, minf.end, FOURCC('s', 't', 'b', 'l'), stbl)) return false;
    if (!findBox(tbl_, stbl.start, stbl.end, FOURCC('s', 't', 's', 'd'), stsd)) return false;
    if (!findBox(tbl_, stbl.start, stbl.end, FOURCC('s', 't', 's', 'z'), stsz)) return false;
    if (!findBox(tbl_, stbl.start, stbl.end, FOURCC('s', 't', 's', 'c'), stsc)) return false;
    bool haveCo = findBox(tbl_, stbl.start, stbl.end, FOURCC('s', 't', 'c', 'o'), stco);
    co64_ = false;
    if (!haveCo) {
        haveCo = findBox(tbl_, stbl.start, stbl.end, FOURCC('c', 'o', '6', '4'), stco);
        co64_ = haveCo;
    }
    if (!haveCo) return false;

    if (!parseSampleDescription(stsd.start, stsd.end)) return false;

    // stsz: version/flags, sample_size, sample_count, [entries]
    uint8_t z[12];
    if (stsz.end - stsz.start < 12 || !readAt(tbl_, stsz.start, z, 12)) return false;
    fixedSize_   = be32(z + 4);
    sampleCount_ = be32(z + 8);
    stsz_.base = stsz.start + 12;
    stsz_.esz  = 4;
    stsz_.count = fixedSize_ ? 0 : sampleCount_;
    if (!fixedSize_ && stsz_.count > (stsz.end - stsz_.base) / 4) {         // truncated table
        stsz_.count = (stsz.end - stsz_.base) / 4;
        sampleCount_ = stsz_.count;
    }
    stsz_.win = szWin_; stsz_.winCap = sizeof(szWin_) / 4;

    // stsc: version/flags, entry_count, [first_chunk, samples_per_chunk, desc_index]
    uint8_t c[8];
    if (stsc.end - stsc.start < 8 || !readAt(tbl_, stsc.start, c, 8)) return false;
    stsc_.base = stsc.start + 8;
    stsc_.esz  = 12;
    stsc_.count = be32(c + 4);
    if (stsc_.count > (stsc.end - stsc_.base) / 12) stsc_.count = (stsc.end - stsc_.base) / 12;
    if (stsc_.count == 0) return false;
    stsc_.win = scWin_; stsc_.winCap = sizeof(scWin_) / 12;

    // stco / co64: version/flags, entry_count, [offsets]
    if (stco.end - stco.start < 8 || !readAt(tbl_, stco.start, c, 8)) return false;
    stco_.base = stco.start + 8;
    stco_.esz  = co64_ ? 8 : 4;
    stco_.count = be32(c + 4);
    if (stco_.count > (stco.end - stco_.base) / stco_.esz) stco_.count = (stco.end - stco_.base) / stco_.esz;
    if (stco_.count == 0) return false;
    stco_.win = coWin_; stco_.winCap = sizeof(coWin_) / stco_.esz;
    return true;
}

bool Mp4Track::parseSampleDescription(uint32_t start, uint32_t end) {
    // stsd: version/flags(4), entry_count(4), then the first sample entry
    uint8_t e[40];
    uint32_t entryStart = start + 8;
    if (end < entryStart + sizeof(e) || !readAt(tbl_, entryStart, e, sizeof(e))) return false;

    uint32_t entrySize = be32(e);
    codec_ = be32(e + 4);
    // AudioSampleEntry: reserved(6) dataRefIdx(2) soundVersion(2) rev(2) vendor(4)
    //                   channels(2) sampleSize(2) compressionId(2) packetSize(2) sampleRate(16.16)
    uint16_t soundVersion = be16(e + 16);
    channels_   = be16(e + 24);
    sampleRate_ = (int)(be32(e + 32) >> 16);

    uint32_t childOff = soundVersion == 0 ? 36 : (soundVersion == 1 ? 52 : 72);
    uint32_t entryEnd = entryStart + entrySize;
    if (entryEnd > end) entryEnd = end;
    if (codec_ == FOURCC('m', 'p', '4', 'a') && entryStart + childOff < entryEnd) {
        parseEsds(entryStart + childOff, entryEnd);
    }
    return true;
}

// esds lives directly in the 'mp4a' entry, or inside a 'wave' box in QuickTime-style files.
void Mp4Track::parseEsds(uint32_t start, uint32_t end) {
    Box esds;
    if (!findBox(tbl_, start, end, FOURCC('e', 's', 'd', 's'), esds)) {
        Box wave;
        if (!findBox(tbl_, start, end, FOURCC('w', 'a', 'v', 'e'), wave) ||
            !findBox(tbl_, wave.start, wave.end, FOURCC('e', 's', 'd', 's'), esds)) return;
    }

    uint8_t d[128];
    uint32_t n = esds.end - esds.start;
    if (n > sizeof(d)) n = sizeof(d);
    if (n < 8 || !readAt(tbl_, esds.start, d, n)) return;

    // Descriptors: tag(1) length(1..4, 7 bits each) ...
    const uint8_t* p = d + 4;                         // skip version/flags
    const uint8_t* e = d + n;
    auto readLen = [&](uint32_t& len) -> bool {
        len = 0;
        for (int i = 0; i < 4; i++) {
            if (p >= e) return false;
            uint8_t b = *p++;
            len = (len << 7) | (b & 0x7F);
            if (!(b & 0x80)) return true;
        }
        return true;
    };
    uint32_t len;

    if (p >= e || *p++ != 0x03 || !readLen(len)) return;               // ES_Descriptor
    if (p + 3 > e) return;
    p += 2;                                                             // ES_ID
    uint8_t flags = *p++;
    if (flags & 0x80) p += 2;
    if (flags & 0x40) { if (p >= e) return; p += 1 + *p; }
    if (flags & 0x20) p += 2;

    if (p >= e || *p++ != 0x04 || !readLen(len)) return;               // DecoderConfigDescriptor
    if (p + 13 > e) return;
    oti_ = p[0];
    p += 13;

    if (p >= e || *p++ != 0x05 || !readLen(len)) return;               // DecoderSpecificInfo = AudioSpecificConfig
    if (len > sizeof(asc_) || p + len > e) return;
    memcpy(asc_, p, len);
    ascLen_ = (int)len;
}

// ── Table access ─────────────────────────────────────────────────────────────
const uint8_t* Mp4Track::entry(Table& t, uint32_t i) {
    if (i >= t.count) return nullptr;
    if (i < t.winFirst || i >= t.winFirst + t.winCount) {
        uint32_t n = t.count - i;
        if (n > t.winCap) n = t.winCap;
        if (!tbl_.seek(t.base + i * t.esz)) return nullptr;
        uint32_t got = (uint32_t)tbl_.read(t.win, n * t.esz) / t.esz;
        if (got == 0) return nullptr;
        t.winFirst = i;
        t.winCount = got;
    }
    return t.win + (i - t.winFirst) * t.esz;
}

bool Mp4Track::setStscEntry(uint32_t idx) {
    const uint8_t* e = entry(stsc_, idx);
    if (!e) return false;
    scSpc_ = be32(e + 4);                              // read before the next entry() may reload the window
    const uint8_t* n = entry(stsc_, idx + 1);
    scNextFirst_ = n ? be32(n) : 0;                    // first chunk covered by the following entry
    scIdx_ = idx;
    return true;
}

bool Mp4Track::beginChunk(uint32_t chunk) {
    if (chunk >= stco_.count) return false;
    while (scNextFirst_ != 0 && chunk + 1 >= scNextFirst_) {      // stsc chunk numbers are 1-based
        if (!setStscEntry(scIdx_ + 1)) return false;
    }
    const uint8_t* e = entry(stco_, chunk);
    if (!e) return false;
    nextOff_   = co64_ ? be32(e + 4) : be32(e);        // co64: low 32 bits (FAT32 files are < 4 GB)
    chunkLeft_ = scSpc_;
    nextChunk_ = chunk + 1;
    return true;
}

bool Mp4Track::nextSample(uint32_t& offset, uint32_t& size) {
    if (sampleIdx_ >= sampleCount_) return false;
    while (chunkLeft_ == 0) {
        if (!beginChunk(nextChunk_)) return false;
    }
    uint32_t sz = fixedSize_;
    if (!sz) {
        const uint8_t* e = entry(stsz_, sampleIdx_);
        if (!e) return false;
        sz = be32(e);
    }
    offset = nextOff_;
    size   = sz;
    nextOff_ += sz;
    chunkLeft_--;
    sampleIdx_++;
    return true;
}
