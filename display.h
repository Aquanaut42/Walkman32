#ifndef DISPLAY_H
#define DISPLAY_H

void showBoot();
void showMainMenu();
void showBrowseMenu();
void showAlbumMenu();
void showArtistMenu();
void showPlaylistMenu();
void showRecordingMenu();
void showStatMenu();
void showBluetoothMenu();
void showBluetoothMenuScanning();
void showPlaybackScreen();

// Draw the battery footer on whatever is on the canvas and send it to the panel
// (for screens drawn outside display.cpp).
void displayFlush();

void showTestMenu1();
void showTestMenu2();
void showTestMenu3();
void showTestMenu4();
void showTestMenu5();

#endif
