#pragma once
#include <Arduino.h>
#include <vector>
#include "playback.h"

void loadArtists();
void loadAlbums();
void loadSongs();
void loadAllSongs();
void loadAllAlbums();

// Returns a queue ready to pass to playback_startQueue()
std::vector<QueueEntry> buildAlbumQueue(const String& artist, const String& album);
QueueEntry buildSingleTrackEntry(const String& title);
