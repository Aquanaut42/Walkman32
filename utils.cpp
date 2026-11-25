#include "utils.h"

String cleanSongName(String fileName, bool keepTrackNumber) {
    int dotPos = fileName.lastIndexOf('.');
    if (dotPos > 0) fileName = fileName.substring(0, dotPos);

    if (keepTrackNumber) return fileName;

    int dashPos = fileName.indexOf('-');
    if (dashPos >= 0) {
        String cleaned = fileName.substring(dashPos + 1);
        cleaned.trim();
        return cleaned;
    }

    fileName.trim();
    return fileName;
}

String truncateString(String str, int maxLen) {
    if (str.length() <= maxLen) {
        return str;
    }
    return str.substring(0, maxLen - 3) + "...";
}
