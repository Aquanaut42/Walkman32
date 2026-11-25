// audio_player.cpp
// All playback goes through bluetooth.cpp (pschatzmann A2DP stack)
// The BT AudioPlayer streams from the SD card automatically.
// These functions update state and control the BT player via bluetooth.h.

#include "audio_player.h"
#include "globals.h"
#include "sd_card.h"
#include "utils.h"
//#include "bluetooth.h"
#include <SD.h>
#include "config.h"

void playSong(String songPath) {
    int lastSlash = songPath.lastIndexOf('/');
    currentSongPath = songPath;
    currentSongName = songPath.substring(lastSlash + 1);
    currentSongName = cleanSongName(currentSongName, false);
    isPlaying = true;
    Serial.println("Now playing: " + songPath);
    // The BT AudioPlayer streams continuously from the SD source folder.
    // For direct track jumping call bluetooth_play_path():
    //bluetooth_play_path(songPath);
}

void playAlbum(int startIndex) {
    playQueue.clear();

    File dir = SD.open(currentPath);
    if (!dir || !dir.isDirectory()) return;

    File file = dir.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            String fileName = String(file.name());
            int lastSlash = fileName.lastIndexOf('/');
            if (lastSlash >= 0) fileName = fileName.substring(lastSlash + 1);

            String lower = fileName;
            lower.toLowerCase();
            if (lower.endsWith(".mp3") || lower.endsWith(".wav") ||
                lower.endsWith(".flac") || lower.endsWith(".m4a")) {
                playQueue.push_back(currentPath + "/" + fileName);
            }
        }
        file.close();
        file = dir.openNextFile();
    }
    dir.close();

    sortStringVector(playQueue);

    currentQueueIndex = startIndex;
    if (playQueue.size() > 0 && currentQueueIndex < (int)playQueue.size()) {
        playSong(playQueue[currentQueueIndex]);
    }
}

void playNext() {
    if (playQueue.size() == 0) return;
    currentQueueIndex++;
    if (currentQueueIndex >= (int)playQueue.size()) currentQueueIndex = 0;
    playSong(playQueue[currentQueueIndex]);
}

void playPrevious() {
    if (playQueue.size() == 0 || digitalRead(BTN_NEXT)) return;
    currentQueueIndex--;
    if (currentQueueIndex < 0) currentQueueIndex = (int)playQueue.size() - 1;
    playSong(playQueue[currentQueueIndex]);
}

void togglePlayPause() {
    //bluetooth_toggle_pause();
}
