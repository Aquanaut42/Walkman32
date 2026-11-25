// navigation.cpp
// Uses the music_db index instead of live SD scanning for all browsing.

#include "navigation.h"
#include "globals.h"
#include "music_db.h"
#include "playback.h"

// ── Helpers: populate currentList from index ──────────────────────────────────

void loadArtists() {
    currentBrowseLevel = LEVEL_ARTISTS;
    currentPath  = "/";
    currentArtist = "";
    currentAlbum  = "";
    currentList.clear();

    for (auto& a : getArtists())
        currentList.push_back(a.name);

    selectedItemIndex = 0;
    scrollOffset = 0;
}

void loadAlbums() {
    currentBrowseLevel = LEVEL_ALBUMS;
    currentPath  = "/" + currentArtist;
    currentAlbum = "";
    currentList.clear();

    auto* albums = getAlbumsForArtist(currentArtist);
    if (albums) {
        for (auto& al : *albums)
            currentList.push_back(al.name);
    }

    selectedItemIndex = 0;
    scrollOffset = 0;
    Serial.println("Loaded albums for: " + currentArtist);
}

void loadSongs() {
    currentBrowseLevel = LEVEL_SONGS;
    currentPath = "/" + currentArtist + "/" + currentAlbum;
    currentList.clear();

    // First entry: Play All shortcut
    if (!currentArtist.isEmpty() && !currentAlbum.isEmpty())
        currentList.push_back(">> Play All <<");

    auto* tracks = getTracksForAlbum(currentArtist, currentAlbum);
    if (tracks) {
        for (auto& t : *tracks)
            currentList.push_back(t.title);
    }

    selectedItemIndex = 0;
    scrollOffset = 0;
    Serial.println("Loaded songs for: " + currentAlbum);
}

// ── Build queue helpers ───────────────────────────────────────────────────────

// Build a QueueEntry vector for an entire album
std::vector<QueueEntry> buildAlbumQueue(const String& artist, const String& album) {
    std::vector<QueueEntry> q;
    auto* tracks = getTracksForAlbum(artist, album);
    if (!tracks) return q;
    for (auto& t : *tracks) {
        QueueEntry e;
        e.title = t.title;
        e.path  = t.path;
        q.push_back(e);
    }
    return q;
}

// Build a QueueEntry for a single track by title within the current album
QueueEntry buildSingleTrackEntry(const String& title) {
    QueueEntry e;
    e.title = title;
    auto* tracks = getTracksForAlbum(currentArtist, currentAlbum);
    if (tracks) {
        for (auto& t : *tracks) {
            if (t.title == title) {
                e.path = t.path;
                break;
            }
        }
    }
    return e;
}

// Legacy stubs kept so display.cpp compiles unchanged
void loadAllSongs()  { loadArtists(); }
void loadAllAlbums() { loadArtists(); }
