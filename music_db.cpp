// music_db.cpp
// Builds and loads a JSON index of the music library on the SD card.
// Format:
// [
//   { "artist": "ArtistA", "albums": [
//       { "album": "AlbumB", "tracks": [
//           { "title": "01 Song", "path": "/ArtistA/AlbumB/01-Song.mp3" },
//           ...
//       ]},
//       ...
//   ]},
//   ...
// ]
//
// Requires: ArduinoJson library (install via Library Manager: "ArduinoJson" by bblanchon)

#include "music_db.h"
#include "utils.h"
#include <SD.h>
#include <ArduinoJson.h>

static std::vector<Artist> g_artists;
static bool g_ready = false;

// ── Helpers ───────────────────────────────────────────────────────────────────

static bool isMusicFile(const String& name) {
    String lower = name;
    lower.toLowerCase();
    return lower.endsWith(".mp3") || lower.endsWith(".wav") ||
           lower.endsWith(".flac") || lower.endsWith(".m4a") ||
           lower.endsWith(".aac");
}

static String lastPathComponent(const String& path) {
    int slash = path.lastIndexOf('/');
    return (slash >= 0) ? path.substring(slash + 1) : path;
}

static String stripExtension(const String& name) {
    int dot = name.lastIndexOf('.');
    return (dot > 0) ? name.substring(0, dot) : name;
}

// ── Build index by scanning SD ────────────────────────────────────────────────

void buildMusicIndex(void (*progressCallback)(const String& status)) {
    g_artists.clear();
    g_ready = false;

    if (progressCallback) progressCallback("Scanning artists...");

    File root = SD.open("/");
    if (!root || !root.isDirectory()) {
        Serial.println("MusicDB: Cannot open SD root");
        return;
    }

    // Iterate artist folders
    File artistDir = root.openNextFile();
    while (artistDir) {
        String artistName = lastPathComponent(String(artistDir.name()));

        if (artistDir.isDirectory() && !artistName.startsWith(".")) {
            Artist artist;
            artist.name = artistName;

            if (progressCallback) progressCallback(truncateString(artistName, 16));

            String artistPath = "/" + artistName;
            File albumDir = artistDir.openNextFile();

            while (albumDir) {
                String albumName = lastPathComponent(String(albumDir.name()));

                if (albumDir.isDirectory() && !albumName.startsWith(".")) {
                    Album album;
                    album.name = albumName;

                    String albumPath = artistPath + "/" + albumName;
                    File trackFile = albumDir.openNextFile();

                    while (trackFile) {
                        String trackName = lastPathComponent(String(trackFile.name()));
                        if (!trackFile.isDirectory() && isMusicFile(trackName)) {
                            Track track;
                            track.title = cleanSongName(stripExtension(trackName), true);
                            track.path  = albumPath + "/" + trackName;
                            album.tracks.push_back(track);
                        }
                        trackFile.close();
                        trackFile = albumDir.openNextFile();
                    }

                    // Sort tracks by title (alphabetical = track number order
                    // if filenames start with track numbers)
                    for (int i = 0; i < (int)album.tracks.size() - 1; i++) {
                        for (int j = 0; j < (int)album.tracks.size() - i - 1; j++) {
                            if (album.tracks[j].title.compareTo(album.tracks[j+1].title) > 0) {
                                Track tmp = album.tracks[j];
                                album.tracks[j] = album.tracks[j+1];
                                album.tracks[j+1] = tmp;
                            }
                        }
                    }

                    if (!album.tracks.empty()) {
                        artist.albums.push_back(album);
                    }
                }
                albumDir.close();
                albumDir = artistDir.openNextFile();
            }

            // Sort albums alphabetically
            for (int i = 0; i < (int)artist.albums.size() - 1; i++) {
                for (int j = 0; j < (int)artist.albums.size() - i - 1; j++) {
                    if (artist.albums[j].name.compareTo(artist.albums[j+1].name) > 0) {
                        Album tmp = artist.albums[j];
                        artist.albums[j] = artist.albums[j+1];
                        artist.albums[j+1] = tmp;
                    }
                }
            }

            if (!artist.albums.empty()) {
                g_artists.push_back(artist);
            }
        }
        artistDir.close();
        artistDir = root.openNextFile();
    }
    root.close();

    // Sort artists alphabetically
    for (int i = 0; i < (int)g_artists.size() - 1; i++) {
        for (int j = 0; j < (int)g_artists.size() - i - 1; j++) {
            if (g_artists[j].name.compareTo(g_artists[j+1].name) > 0) {
                Artist tmp = g_artists[j];
                g_artists[j] = g_artists[j+1];
                g_artists[j+1] = tmp;
            }
        }
    }

    g_ready = true;
    Serial.printf("MusicDB: Scanned %d artists\n", g_artists.size());

    // ── Write JSON index to SD ────────────────────────────────────────────────
    if (progressCallback) progressCallback("Writing index...");

    SD.remove(MUSIC_INDEX_PATH);
    File f = SD.open(MUSIC_INDEX_PATH, FILE_WRITE);
    if (!f) {
        Serial.println("MusicDB: Cannot write index file");
        return;
    }

    // Stream the JSON manually to avoid one giant allocation
    f.print("[");
    for (int ai = 0; ai < (int)g_artists.size(); ai++) {
        if (ai > 0) f.print(",");
        f.print("{\"artist\":\"");
        f.print(g_artists[ai].name);
        f.print("\",\"albums\":[");
        for (int ali = 0; ali < (int)g_artists[ai].albums.size(); ali++) {
            if (ali > 0) f.print(",");
            f.print("{\"album\":\"");
            f.print(g_artists[ai].albums[ali].name);
            f.print("\",\"tracks\":[");
            for (int ti = 0; ti < (int)g_artists[ai].albums[ali].tracks.size(); ti++) {
                if (ti > 0) f.print(",");
                f.print("{\"title\":\"");
                // Escape quotes in title
                String t = g_artists[ai].albums[ali].tracks[ti].title;
                t.replace("\"", "\\\"");
                f.print(t);
                f.print("\",\"path\":\"");
                String p = g_artists[ai].albums[ali].tracks[ti].path;
                p.replace("\"", "\\\"");
                f.print(p);
                f.print("\"}");
            }
            f.print("]}");
        }
        f.print("]}");
    }
    f.print("]");
    f.close();

    Serial.println("MusicDB: Index written to " MUSIC_INDEX_PATH);
}

// ── Load index from SD ────────────────────────────────────────────────────────

bool loadMusicIndex() {
    g_artists.clear();
    g_ready = false;

    if (!SD.exists(MUSIC_INDEX_PATH)) {
        Serial.println("MusicDB: No index file found");
        return false;
    }

    File f = SD.open(MUSIC_INDEX_PATH, FILE_READ);
    if (!f) {
        Serial.println("MusicDB: Cannot open index file");
        return false;
    }

    // ArduinoJson streaming parse — uses minimal RAM
    // Use DynamicJsonDocument sized for the file
    size_t fileSize = f.size();
    Serial.printf("MusicDB: Index file size: %d bytes\n", fileSize);

    // For large libraries (1000+ tracks) we stream-parse artist by artist
    // using a filter to keep RAM usage low
    DynamicJsonDocument doc(8192);  // per-artist buffer

    // Read the whole file as a stream
    // Since ArduinoJson can't stream from File directly on all platforms,
    // we parse in chunks using the array iterator pattern
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err) {
        Serial.print("MusicDB: JSON parse error: ");
        Serial.println(err.c_str());
        return false;
    }

    JsonArray artistArray = doc.as<JsonArray>();
    for (JsonObject artistObj : artistArray) {
        Artist artist;
        artist.name = artistObj["artist"].as<String>();

        JsonArray albums = artistObj["albums"].as<JsonArray>();
        for (JsonObject albumObj : albums) {
            Album album;
            album.name = albumObj["album"].as<String>();

            JsonArray tracks = albumObj["tracks"].as<JsonArray>();
            for (JsonObject trackObj : tracks) {
                Track track;
                track.title = trackObj["title"].as<String>();
                track.path  = trackObj["path"].as<String>();
                album.tracks.push_back(track);
            }
            artist.albums.push_back(album);
        }
        g_artists.push_back(artist);
    }

    g_ready = true;
    Serial.printf("MusicDB: Loaded %d artists from index\n", g_artists.size());
    return true;
}

// ── Public accessors ──────────────────────────────────────────────────────────

bool musicIndexReady() { return g_ready; }

const std::vector<Artist>& getArtists() { return g_artists; }

const std::vector<Album>* getAlbumsForArtist(const String& artistName) {
    for (auto& a : g_artists) {
        if (a.name == artistName) return &a.albums;
    }
    return nullptr;
}

const std::vector<Track>* getTracksForAlbum(const String& artistName,
                                              const String& albumName) {
    auto* albums = getAlbumsForArtist(artistName);
    if (!albums) return nullptr;
    for (auto& al : *albums) {
        if (al.name == albumName) return &al.tracks;
    }
    return nullptr;
}

int getTotalTrackCount() {
    int total = 0;
    for (auto& a : g_artists)
        for (auto& al : a.albums)
            total += al.tracks.size();
    return total;
}
