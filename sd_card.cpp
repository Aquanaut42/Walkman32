#include "sd_card.h"
#include "globals.h"
#include "config.h"
#include "utils.h"
#include <SPI.h>
#include <SD.h>

SPIClass SD_SPI(HSPI);

bool initSDCard() {
    SD_SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

    if (!SD.begin(SD_CS, SD_SPI)) {
        Serial.println("SD init failed");
        return false;
    }
    return true;
}

void listDirectories(String path, std::vector<String>& items) {
    items.clear();

    if (!sdCardReady) {
        Serial.println("SD card not ready");
        return;
    }

    if (path.length() > 1 && path.endsWith("/")) {
        path = path.substring(0, path.length() - 1);
    }

    Serial.println("Listing directories in: " + path);
    File root = SD.open(path);

    if (!root) {
        Serial.println("Failed to open directory: " + path);
        return;
    }

    if (!root.isDirectory()) {
        Serial.println("Not a directory: " + path);
        root.close();
        return;
    }

    File file = root.openNextFile();
    while (file) {
        if (file.isDirectory()) {
            String name = String(file.name());

            int lastSlash = name.lastIndexOf('/');
            if (lastSlash >= 0) {
                name = name.substring(lastSlash + 1);
            }

            if (!name.startsWith(".")) {
                items.push_back(name);
                Serial.println("Dir: " + name);
            }
        }

        file.close();
        file = root.openNextFile();
    }
    root.close();

    sortStringVector(items);

    Serial.printf("Found %d directories\n", items.size());
}

void listMusicFiles(String path, std::vector<String>& items) {
    items.clear();

    if (!sdCardReady) {
        Serial.println("SD card not ready");
        return;
    }

    if (path.length() > 1 && path.endsWith("/")) {
        path = path.substring(0, path.length() - 1);
    }

    Serial.println("Listing music files in: " + path);
    File root = SD.open(path);

    if (!root) {
        Serial.println("Failed to open directory: " + path);
        return;
    }

    if (!root.isDirectory()) {
        Serial.println("Not a directory: " + path);
        root.close();
        return;
    }

    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            String fileName = String(file.name());

            int lastSlash = fileName.lastIndexOf('/');
            if (lastSlash >= 0) {
                fileName = fileName.substring(lastSlash + 1);
            }

            String lowerName = fileName;
            lowerName.toLowerCase();

            if (lowerName.endsWith(".mp3") ||
                lowerName.endsWith(".wav") ||
                lowerName.endsWith(".flac") ||
                lowerName.endsWith(".m4a") ||
                lowerName.endsWith(".aac")) {
                String cleanName = cleanSongName(fileName);
            items.push_back(cleanName);
            Serial.println("File: " + cleanName);
                }
        }

        file.close();
        file = root.openNextFile();
    }
    root.close();

    sortStringVector(items);

    Serial.printf("Found %d music files\n", items.size());
}

void listMusicFilesWithTrackNumbers(String path, std::vector<String>& items) {
    items.clear();

    if (!sdCardReady) {
        Serial.println("SD card not ready");
        return;
    }

    if (path.length() > 1 && path.endsWith("/")) {
        path = path.substring(0, path.length() - 1);
    }

    Serial.println("Listing music files in: " + path);
    File root = SD.open(path);

    if (!root) {
        Serial.println("Failed to open directory: " + path);
        return;
    }

    if (!root.isDirectory()) {
        Serial.println("Not a directory: " + path);
        root.close();
        return;
    }

    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            String fileName = String(file.name());

            int lastSlash = fileName.lastIndexOf('/');
            if (lastSlash >= 0) {
                fileName = fileName.substring(lastSlash + 1);
            }

            String lowerName = fileName;
            lowerName.toLowerCase();

            if (lowerName.endsWith(".mp3") ||
                lowerName.endsWith(".wav") ||
                lowerName.endsWith(".flac") ||
                lowerName.endsWith(".m4a") ||
                lowerName.endsWith(".aac")) {
                String cleanName = cleanSongName(fileName, true);
            items.push_back(cleanName);
            Serial.println("File: " + cleanName);
                }
        }

        file.close();
        file = root.openNextFile();
    }
    root.close();

    sortStringVector(items);

    Serial.printf("Found %d music files\n", items.size());
}

int countTotalSongs() {
    int total = 0;

    std::vector<String> artists;
    listDirectories("/", artists);

    for (const String& artist : artists) {
        String artistPath = "/" + artist;

        std::vector<String> albums;
        listDirectories(artistPath, albums);

        for (const String& album : albums) {
            String albumPath = artistPath + "/" + album;

            std::vector<String> songs;
            listMusicFiles(albumPath, songs);
            total += songs.size();
        }
    }

    return total;
}

void listAllSongs(std::vector<String>& items) {
    items.clear();

    if (!sdCardReady) {
        Serial.println("SD card not ready");
        return;
    }

    Serial.println("Listing all songs...");

    std::vector<String> artists;
    listDirectories("/", artists);

    for (const String& artist : artists) {
        String artistPath = "/" + artist;

        std::vector<String> albums;
        listDirectories(artistPath, albums);

        for (const String& album : albums) {
            String albumPath = artistPath + "/" + album;

            std::vector<String> songs;
            listMusicFiles(albumPath, songs);

            for (const String& song : songs) {
                items.push_back(song);
            }
        }
    }

    sortStringVector(items);

    Serial.printf("Found %d total songs\n", items.size());
}

void listAllAlbums(std::vector<String>& items) {
    items.clear();

    if (!sdCardReady) {
        Serial.println("SD card not ready");
        return;
    }

    Serial.println("Listing all albums...");

    std::vector<String> artists;
    listDirectories("/", artists);

    for (const String& artist : artists) {
        String artistPath = "/" + artist;

        std::vector<String> albums;
        listDirectories(artistPath, albums);

        for (const String& album : albums) {
            items.push_back(album);
        }
    }

    sortStringVector(items);

    Serial.printf("Found %d total albums\n", items.size());
}

String findFileRecursive(String startDir, String targetCleanName) {
    File root = SD.open(startDir);
    if (!root || !root.isDirectory()) return "";

    File file = root.openNextFile();
    while (file) {
        String fileName = String(file.name());

        int lastSlash = fileName.lastIndexOf('/');
        String simpleName = (lastSlash >= 0) ? fileName.substring(lastSlash + 1) : fileName;

        if (file.isDirectory()) {
            if (!simpleName.startsWith(".")) {
                String subDir = startDir + (startDir == "/" ? "" : "/") + simpleName;
                String result = findFileRecursive(subDir, targetCleanName);
                if (result != "") {
                    file.close();
                    root.close();
                    return result;
                }
            }
        } else {
            if (cleanSongName(simpleName, false).equals(targetCleanName)) {
                String fullPath = startDir + (startDir == "/" ? "" : "/") + simpleName;
                file.close();
                root.close();
                return fullPath;
            }
        }
        file.close();
        file = root.openNextFile();
    }
    root.close();
    return "";
}

String getFullSongPath(String songName) {
    String target = cleanSongName(songName, false);
    Serial.print("Global search for: "); Serial.println(target);

    String path = findFileRecursive(currentPath, target);

    if (path == "" && currentPath != "/") {
        path = findFileRecursive("/", target);
    }

    if (path == "") {
        Serial.print("Error: Could not find file for "); Serial.println(target);
    }
    return path;
}

void sortStringVector(std::vector<String>& items) {
    for (int i = 0; i < items.size() - 1; i++) {
        for (int j = 0; j < items.size() - i - 1; j++) {
            if (items[j].compareTo(items[j + 1]) > 0) {
                String temp = items[j];
                items[j] = items[j + 1];
                items[j + 1] = temp;
            }
        }
    }
}
