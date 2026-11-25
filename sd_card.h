#ifndef SD_CARD_H
#define SD_CARD_H

#include <Arduino.h>
#include <vector>

bool initSDCard();
void listDirectories(String path, std::vector<String>& items);
void listMusicFiles(String path, std::vector<String>& items);
void listMusicFilesWithTrackNumbers(String path, std::vector<String>& items);
int countTotalSongs();
void listAllSongs(std::vector<String>& items);
void listAllAlbums(std::vector<String>& items);
String findFileRecursive(String startDir, String targetCleanName);
String getFullSongPath(String songName);
void sortStringVector(std::vector<String>& items);

#endif
