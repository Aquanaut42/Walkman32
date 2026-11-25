#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>

String cleanSongName(String fileName, bool keepTrackNumber = false);
String truncateString(String str, int maxLen);

#endif
