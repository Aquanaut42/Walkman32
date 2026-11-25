#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include <vector>
#include <RotaryEncoder.h>
// Audio.h (ESP32-audioI2S) REMOVED — using pschatzmann A2DP stack instead
#include "DEV_Config.h"
#include "enum.h"

// Display buffer
extern UBYTE *BlackImage;

// Rotary encoder
extern RotaryEncoder *encoder;
extern int last_rotary;

// SD Card state
extern bool sdCardReady;
extern std::vector<String> currentList;
extern int scrollOffset;

// Navigation state
extern String currentArtist;
extern String currentAlbum;
extern String currentPath;
extern BrowseLevel currentBrowseLevel;

// Audio playback (no Audio object — BT player handles this)
extern bool isPlaying;
extern String currentSongPath;
extern String currentSongName;
extern std::vector<String> playQueue;
extern int currentQueueIndex;

// Bluetooth state
extern bool bluetoothEnabled;
extern bool bluetoothConnected;
extern String bluetoothDeviceName;
extern std::vector<String> bluetoothDevices;
extern int selectedBluetoothDevice;

// State
extern int menuItem;
extern int selectedItemIndex;
extern PageEnum currentPage;
extern PageEnum returnToPage;

// Button debouncing
extern unsigned long lastEncoderPress;
extern unsigned long lastPrevPress;
extern unsigned long lastNextPress;
extern unsigned long lastPlayPress;
extern unsigned long lastRecPress;
extern unsigned long lastReturnPress;

// Button state tracking
extern bool lastEncoderState;

#endif
