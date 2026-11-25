#pragma once
#include <Arduino.h>
#include <vector>

// ── Data structures ───────────────────────────────────────────────────────────

struct Track {
    String title;   // display name (cleaned, no extension)
    String path;    // full SD path e.g. /ArtistA/AlbumB/01-Song.mp3
};

struct Album {
    String name;
    std::vector<Track> tracks;
};

struct Artist {
    String name;
    std::vector<Album> albums;
};

// ── Index path on SD card ─────────────────────────────────────────────────────
#define MUSIC_INDEX_PATH "/walkman32_index.json"

// ── Public API ────────────────────────────────────────────────────────────────

// Load index from SD card. Returns true if index existed and loaded OK.
// If index missing or corrupt, returns false (caller should call buildIndex).
bool loadMusicIndex();

// Scan SD card and rebuild the index. Writes JSON to MUSIC_INDEX_PATH.
// Shows progress on OLED via progressCallback (optional).
void buildMusicIndex(void (*progressCallback)(const String& status) = nullptr);

// True if the in-memory index has been populated.
bool musicIndexReady();

// Access the loaded data
const std::vector<Artist>& getArtists();

// Get all tracks for an artist+album (returns empty if not found)
const std::vector<Track>* getTracksForAlbum(const String& artistName,
                                              const String& albumName);

// Get all albums for an artist
const std::vector<Album>* getAlbumsForArtist(const String& artistName);

// Total track count across whole library
int getTotalTrackCount();
