#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <Arduino.h>

void playSong(String songPath);
void playAlbum(int startIndex = 0);
void playNext();
void playPrevious();
void togglePlayPause();

#endif
