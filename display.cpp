// display.cpp
#include "display.h"
#include "globals.h"
#include "config.h"
#include "sd_card.h"
#include "utils.h"
#include "OLED_Driver.h"
#include "GUI_Paint.h"
#include <SD.h>
#include "music_db.h"
#include "playback.h"
#include "battery.h"


// ── Battery footer ────────────────────────────────────────────────────────────
// Bottom row of every screen (the canvas is 64 x 128): battery icon and percentage.
// Adapted from the ESP32_SD_BT_Player battery indicator.
static void drawBatteryFooter() {
    const int y = 118;
    const int pct = battery_percent();

    // Icon: 16 x 8 body with a 2 px nub; the fill is up to 12 px wide inside it.
    Paint_DrawRectangle(1, y, 16, y + 7, WHITE, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    Paint_DrawRectangle(17, y + 2, 18, y + 5, WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL);

    char text[8];
    if (pct >= 0) {
        int fillW = (pct * 12 + 50) / 100;
        if (pct > 0 && fillW < 1) fillW = 1;
        if (fillW > 0) Paint_DrawRectangle(3, y + 2, 3 + fillW - 1, y + 5, WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
        snprintf(text, sizeof(text), "%d%%", pct);
    } else {
        snprintf(text, sizeof(text), "--%%");              // no battery sense input
    }
    Paint_DrawString_EN(22, y, text, &Font8, BLACK, WHITE);
}

// Every screen ends here: footer, then the frame goes to the panel.
static void finishFrame() {
    drawBatteryFooter();
    OLED_2IN42_Display(BlackImage);
}

void displayFlush() { finishFrame(); }

void showTestMenu1() {
    Paint_NewImage(BlackImage, OLED_2IN42_WIDTH, OLED_2IN42_HEIGHT, 0, BLACK);
    Paint_SelectImage(BlackImage);
    Paint_Clear(BLACK);

    Paint_DrawString_EN(0, 5, "Fieldwalk", &Font12, BLACK, WHITE);
    Paint_DrawLine(0, 18, 127, 18, WHITE, DOT_PIXEL_1X1, LINE_STYLE_SOLID);

    Paint_DrawString_EN(5, 25, "  Return", &Font8, BLACK, WHITE);

    int cursorY = 25 + (menuItem - 1) * 10;
    Paint_DrawString_EN(5, cursorY, ">", &Font8, BLACK, WHITE);

    finishFrame();
    //delay(5000);
}
void showTestMenu2() {
    Paint_NewImage(BlackImage, OLED_2IN42_WIDTH, OLED_2IN42_HEIGHT, 0, BLACK);
    Paint_SelectImage(BlackImage);
    Paint_Clear(BLACK);

    Paint_DrawString_EN(0, 5, "Fieldwalk", &Font12, BLACK, WHITE);
    Paint_DrawLine(0, 18, 127, 18, WHITE, DOT_PIXEL_1X1, LINE_STYLE_SOLID);

    Paint_DrawString_EN(5, 25, "  Prev", &Font8, BLACK, WHITE);

    int cursorY = 25 + (menuItem - 1) * 10;
    Paint_DrawString_EN(5, cursorY, ">", &Font8, BLACK, WHITE);

    finishFrame();
    //delay(5000);
}
void showTestMenu3() {
    Paint_NewImage(BlackImage, OLED_2IN42_WIDTH, OLED_2IN42_HEIGHT, 0, BLACK);
    Paint_SelectImage(BlackImage);
    Paint_Clear(BLACK);

    Paint_DrawString_EN(0, 5, "Fieldwalk", &Font12, BLACK, WHITE);
    Paint_DrawLine(0, 18, 127, 18, WHITE, DOT_PIXEL_1X1, LINE_STYLE_SOLID);

    Paint_DrawString_EN(5, 25, "  Next", &Font8, BLACK, WHITE);

    int cursorY = 25 + (menuItem - 1) * 10;
    Paint_DrawString_EN(5, cursorY, ">", &Font8, BLACK, WHITE);

    finishFrame();
    //delay(5000);
}
void showTestMenu4() {
    Paint_NewImage(BlackImage, OLED_2IN42_WIDTH, OLED_2IN42_HEIGHT, 0, BLACK);
    Paint_SelectImage(BlackImage);
    Paint_Clear(BLACK);

    Paint_DrawString_EN(0, 5, "Fieldwalk", &Font12, BLACK, WHITE);
    Paint_DrawLine(0, 18, 127, 18, WHITE, DOT_PIXEL_1X1, LINE_STYLE_SOLID);

    Paint_DrawString_EN(5, 25, "  Play", &Font8, BLACK, WHITE);

    int cursorY = 25 + (menuItem - 1) * 10;
    Paint_DrawString_EN(5, cursorY, ">", &Font8, BLACK, WHITE);

    finishFrame();
    //delay(5000);
}
void showTestMenu5() {
    Paint_NewImage(BlackImage, OLED_2IN42_WIDTH, OLED_2IN42_HEIGHT, 0, BLACK);
    Paint_SelectImage(BlackImage);
    Paint_Clear(BLACK);

    Paint_DrawString_EN(0, 5, "Fieldwalk", &Font12, BLACK, WHITE);
    Paint_DrawLine(0, 18, 127, 18, WHITE, DOT_PIXEL_1X1, LINE_STYLE_SOLID);

    Paint_DrawString_EN(5, 25, "  Rec", &Font8, BLACK, WHITE);

    int cursorY = 25 + (menuItem - 1) * 10;
    Paint_DrawString_EN(5, cursorY, ">", &Font8, BLACK, WHITE);

    finishFrame();
    //delay(5000);
}

void showBoot() {
    Paint_Clear(BLACK);
    Paint_NewImage(BlackImage, OLED_2IN42_WIDTH, OLED_2IN42_HEIGHT, 270, WHITE);
    Paint_SelectImage(BlackImage);
    Paint_Clear(BLACK);

    Serial.println("Showing startup screen...");
    Paint_SetRotate(90);

    int bootArt = random(0, 6);
    switch (bootArt) {
        case 0:
            Paint_DrawString_EN(10, 10, "  _________         ", &Font8, BLACK, WHITE);
            Paint_DrawString_EN(10, 20, "     _____/__  _  __", &Font8, BLACK, WHITE);
            Paint_DrawString_EN(10, 30, "      __)     /  / /", &Font8, BLACK, WHITE);
            Paint_DrawString_EN(10, 40, "     |            / ", &Font8, BLACK, WHITE);
            Paint_DrawString_EN(10, 50, "  ___|        / _/  ", &Font8, BLACK, WHITE);
            break;
        case 1:
            Paint_DrawString_EN(10, 10, "    ____  _  _  ", &Font8, BLACK, WHITE);
            Paint_DrawString_EN(10, 20, "   (  __)/ )( \\ ", &Font8, BLACK, WHITE);
            Paint_DrawString_EN(10, 30, "    ) _) \\ /\\ / ", &Font8, BLACK, WHITE);
            Paint_DrawString_EN(10, 40, "   (__)  (_/\\_) ", &Font8, BLACK, WHITE);
            break;
        case 2:
            Paint_DrawString_EN(10, 10, "    _____        ", &Font8, BLACK, WHITE);
            Paint_DrawString_EN(10, 20, "   |   __|_ _ _ ", &Font8, BLACK, WHITE);
            Paint_DrawString_EN(10, 30, "   |   __| | | |", &Font8, BLACK, WHITE);
            Paint_DrawString_EN(10, 40, "   |__|  |_____|", &Font8, BLACK, WHITE);
            break;
        case 3:
            Paint_DrawString_EN(10, 10, "     ______        ", &Font8, BLACK, WHITE);
            Paint_DrawString_EN(10, 20, "    / ____/      __", &Font8, BLACK, WHITE);
            Paint_DrawString_EN(10, 30, "   / /_  | | /| / /", &Font8, BLACK, WHITE);
            Paint_DrawString_EN(10, 40, "  / __/  | |/ |/ / ", &Font8, BLACK, WHITE);
            Paint_DrawString_EN(10, 50, " /_/     |__/|__/  ", &Font8, BLACK, WHITE);
            break;
        case 4:
            Paint_DrawString_EN(10, 10, ",------.           ", &Font8, BLACK, WHITE);
            Paint_DrawString_EN(10, 20, "|  .---',--.   ,--.", &Font8, BLACK, WHITE);
            Paint_DrawString_EN(10, 30, "|  `--, |  |.'.|  |", &Font8, BLACK, WHITE);
            Paint_DrawString_EN(10, 40, "|  |`   |   .'.   |", &Font8, BLACK, WHITE);
            Paint_DrawString_EN(10, 50, "`--'    '--'   '--'", &Font8, BLACK, WHITE);
            break;
        case 5:
            Paint_DrawString_EN(10, 10, " _______           ", &Font8, BLACK, WHITE);
            Paint_DrawString_EN(10, 20, "|    ___|.--.--.--.", &Font8, BLACK, WHITE);
            Paint_DrawString_EN(10, 30, "|    ___||  |  |  |", &Font8, BLACK, WHITE);
            Paint_DrawString_EN(10, 40, "|___|    |________|", &Font8, BLACK, WHITE);
            break;
    }

    Paint_SetRotate(0);
    OLED_2IN42_Display(BlackImage);
    delay(1500);
}

void showMainMenu() {
    Paint_NewImage(BlackImage, OLED_2IN42_WIDTH, OLED_2IN42_HEIGHT, 0, BLACK);
    Paint_SelectImage(BlackImage);
    Paint_Clear(BLACK);

    Paint_DrawString_EN(0, 5, "Fieldwalk", &Font12, BLACK, WHITE);
    Paint_DrawLine(0, 18, 127, 18, WHITE, DOT_PIXEL_1X1, LINE_STYLE_SOLID);

    Paint_DrawString_EN(5, 25, "  Songs", &Font8, BLACK, WHITE);
    Paint_DrawString_EN(5, 35, "  Albums", &Font8, BLACK, WHITE);
    Paint_DrawString_EN(5, 45, "  Artists", &Font8, BLACK, WHITE);
    Paint_DrawString_EN(5, 55, "  Playlists", &Font8, BLACK, WHITE);
    Paint_DrawString_EN(5, 65, "  Recordings", &Font8, BLACK, WHITE);
    Paint_DrawString_EN(5, 75, "  Stats", &Font8, BLACK, WHITE);
    Paint_DrawString_EN(5, 85, "  Bluetooth", &Font8, BLACK, WHITE);

    int cursorY = 25 + (menuItem - 1) * 10;
    Paint_DrawString_EN(5, cursorY, ">", &Font8, BLACK, WHITE);

    finishFrame();
}

void showBrowseMenu() {
    Paint_NewImage(BlackImage, OLED_2IN42_WIDTH, OLED_2IN42_HEIGHT, 0, BLACK);
    Paint_SelectImage(BlackImage);
    Paint_Clear(BLACK);

    String header = "";
    if (currentPage == PAGE_SONGS && currentArtist == "" && currentAlbum == "") {
        header = "All Songs";
    } else if (currentPage == PAGE_ALBUMS && currentArtist == "") {
        header = "All Albums";
    } else if (currentPage == PAGE_ARTISTS) {
        if (currentArtist == "") {
            header = "Artists";
        } else if (currentAlbum == "") {
            header = truncateString(currentArtist, 15);
        } else {
            header = truncateString(currentAlbum, 15);
        }
    } else {
        switch(currentBrowseLevel) {
            case LEVEL_ARTISTS:
                header = "Artists";
                break;
            case LEVEL_ALBUMS:
                header = truncateString(currentArtist, 15);
                break;
            case LEVEL_SONGS:
                header = truncateString(currentAlbum, 15);
                break;
        }
    }

    Paint_DrawString_EN(0, 5, header.c_str(), &Font12, BLACK, WHITE);
    Paint_DrawLine(0, 18, 127, 18, WHITE, DOT_PIXEL_1X1, LINE_STYLE_SOLID);

    if (!sdCardReady) {
        Paint_DrawString_EN(5, 30, "No SD Card!", &Font8, BLACK, WHITE);
    } else if (currentList.size() == 0) {
        Paint_DrawString_EN(5, 30, "No items found", &Font8, BLACK, WHITE);
    } else {
        int startIdx = scrollOffset;
        int endIdx = min(startIdx + MAX_VISIBLE_ITEMS, (int)currentList.size());

        for (int i = startIdx; i < endIdx; i++) {
            int yPos = 25 + (i - startIdx) * 10;

            String itemName = currentList[i];
            if (currentBrowseLevel == LEVEL_SONGS && currentArtist != "" && currentAlbum != "") {
                itemName = cleanSongName(itemName, false);
            }

            String displayName = truncateString(itemName, 14);

            if (i == selectedItemIndex) {
                Paint_DrawString_EN(5, yPos, ">", &Font8, BLACK, WHITE);
            }
            Paint_DrawString_EN(13, yPos, displayName.c_str(), &Font8, BLACK, WHITE);
        }

        if (currentList.size() > MAX_VISIBLE_ITEMS) {
            char scrollInfo[16];
            sprintf(scrollInfo, "%d/%d", selectedItemIndex + 1, currentList.size());
            Paint_DrawString_EN(85, 85, scrollInfo, &Font8, BLACK, WHITE);
        }
    }

    finishFrame();
}

void showAlbumMenu() {
    showBrowseMenu();
}

void showArtistMenu() {
    showBrowseMenu();
}

void showPlaylistMenu() {
    Paint_NewImage(BlackImage, OLED_2IN42_WIDTH, OLED_2IN42_HEIGHT, 0, BLACK);
    Paint_SelectImage(BlackImage);
    Paint_Clear(BLACK);
    Paint_DrawString_EN(0, 5, "Playlists", &Font12, BLACK, WHITE);
    Paint_DrawString_EN(5, 30, "Coming soon...", &Font8, BLACK, WHITE);
    finishFrame();
}

void showRecordingMenu() {
    Paint_NewImage(BlackImage, OLED_2IN42_WIDTH, OLED_2IN42_HEIGHT, 0, BLACK);
    Paint_SelectImage(BlackImage);
    Paint_Clear(BLACK);
    Paint_DrawString_EN(0, 5, "Recording", &Font12, BLACK, WHITE);
    Paint_DrawString_EN(5, 30, "Coming soon...", &Font8, BLACK, WHITE);
    finishFrame();
}

void showStatMenu() {
    Paint_NewImage(BlackImage, OLED_2IN42_WIDTH, OLED_2IN42_HEIGHT, 0, BLACK);
    Paint_SelectImage(BlackImage);
    Paint_Clear(BLACK);
    Paint_DrawString_EN(0, 5, "Stats", &Font12, BLACK, WHITE);
    Paint_DrawLine(0, 18, 127, 18, WHITE, DOT_PIXEL_1X1, LINE_STYLE_SOLID);

    if (sdCardReady) {
        char buffer[32];

        sprintf(buffer, "Artists:%d", (int)getArtists().size());
        Paint_DrawString_EN(5, 25, buffer, &Font8, BLACK, WHITE);

        sprintf(buffer, "Tracks:%d", getTotalTrackCount());
        Paint_DrawString_EN(5, 35, buffer, &Font8, BLACK, WHITE);

        uint64_t cardSize = SD.cardSize() / (1024 * 1024);
        sprintf(buffer, "SD:%lluMB", cardSize);
        Paint_DrawString_EN(5, 50, buffer, &Font8, BLACK, WHITE);

        uint64_t usedSpace = SD.usedBytes() / (1024 * 1024);
        sprintf(buffer, "Used:%lluMB", usedSpace);
        Paint_DrawString_EN(5, 60, buffer, &Font8, BLACK, WHITE);

        Paint_DrawString_EN(5, 78, "Play=Rebuild index", &Font8, BLACK, WHITE);
    } else {
        Paint_DrawString_EN(5, 30, "No SD Card", &Font8, BLACK, WHITE);
    }

    finishFrame();
}

void showPlaybackScreen() {
    Paint_NewImage(BlackImage, OLED_2IN42_WIDTH, OLED_2IN42_HEIGHT, 0, BLACK);
    Paint_SelectImage(BlackImage);
    Paint_Clear(BLACK);

    Paint_DrawString_EN(0, 5, "Now Playing", &Font12, BLACK, WHITE);
    Paint_DrawLine(0, 18, 127, 18, WHITE, DOT_PIXEL_1X1, LINE_STYLE_SOLID);

    String displayName = truncateString(playback_currentTitle(), 18);
    Paint_DrawString_EN(5, 25, displayName.c_str(), &Font8, BLACK, WHITE);

    String artistInfo = truncateString(currentArtist, 18);
    Paint_DrawString_EN(5, 35, artistInfo.c_str(), &Font8, BLACK, WHITE);

    String albumInfo = truncateString(currentAlbum, 18);
    Paint_DrawString_EN(5, 45, albumInfo.c_str(), &Font8, BLACK, WHITE);

    Paint_DrawString_EN(5, 60,
        playback_isPlaying() ? ">> Playing" : (playback_isPaused() ? "|| Paused" : "-- Stopped"),
        &Font8, BLACK, WHITE);

    Paint_DrawRectangle(5, 72, 115, 78, WHITE, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);

    if (playback_queueSize() > 1) {
        char queueStr[20];
        sprintf(queueStr, "Track %d/%d",
                playback_queueIndex() + 1,
                playback_queueSize());
        Paint_DrawString_EN(5, 85, queueStr, &Font8, BLACK, WHITE);
    }

    // BT status
    if (playback_isBluetoothEnabled()) {
        Paint_DrawString_EN(5, 95,
            playback_isBluetoothConnected() ? "BT:Connected" : "BT:Searching",
            &Font8, BLACK, WHITE);
    }

    finishFrame();
}

void showBluetoothMenu() {
    Paint_NewImage(BlackImage, OLED_2IN42_WIDTH, OLED_2IN42_HEIGHT, 0, BLACK);
    Paint_SelectImage(BlackImage);
    Paint_Clear(BLACK);

    Paint_DrawString_EN(0, 5, "Bluetooth", &Font12, BLACK, WHITE);
    Paint_DrawLine(0, 18, 127, 18, WHITE, DOT_PIXEL_1X1, LINE_STYLE_SOLID);

    if (!bluetoothEnabled) { 
        // If bluetooth is disabled
        Paint_DrawString_EN(5, 25, "Status: OFF", &Font8, BLACK, WHITE);
        Paint_DrawString_EN(0, 35, ">enable", &Font8, BLACK, WHITE);
    } else if (bluetoothDevices.size() == 0) { 
        // if bluetooth is enabled
        Paint_DrawString_EN(5, 25, "Status: ON", &Font8, BLACK, WHITE);
        Paint_DrawString_EN(0, 35, ">Scan devices", &Font8, BLACK, WHITE);
        Paint_DrawString_EN(5, 45, "No device", &Font8, BLACK, WHITE);
    } else {
        // Show device list
        Paint_DrawString_EN(5, 25, "Select device:", &Font8, BLACK, WHITE);

        int startIdx = max(0, selectedBluetoothDevice - 4);
        int endIdx = min(startIdx + 5, (int)bluetoothDevices.size());

        for (int i = startIdx; i < endIdx; i++) {
            int yPos = 35 + (i - startIdx) * 10;

            String displayName = truncateString(bluetoothDevices[i], 14);

            if (i == selectedBluetoothDevice) {
                Paint_DrawString_EN(5, yPos, ">", &Font8, BLACK, WHITE);
            }
            Paint_DrawString_EN(13, yPos, displayName.c_str(), &Font8, BLACK, WHITE);
        }

        Paint_DrawString_EN(5, 90, "PLAY:Conn REC:Scan", &Font8, BLACK, WHITE);
    }

    if (bluetoothConnected) {
        Paint_DrawString_EN(5, 100, "Connected!", &Font8, BLACK, WHITE);
    }

    finishFrame();
}

void showBluetoothMenuScanning() {
    Paint_NewImage(BlackImage, OLED_2IN42_WIDTH, OLED_2IN42_HEIGHT, 0, BLACK);
    Paint_SelectImage(BlackImage);
    Paint_Clear(BLACK);

    Paint_DrawString_EN(0, 5, "Bluetooth", &Font12, BLACK, WHITE);
    Paint_DrawLine(0, 18, 127, 18, WHITE, DOT_PIXEL_1X1, LINE_STYLE_SOLID);

    Paint_DrawString_EN(5, 100, "Scanning...", &Font8, BLACK, WHITE);

    finishFrame();
}
