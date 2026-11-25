#include "globals.h"
#include "DEV_Config.h"

// Display buffer
UBYTE *BlackImage = NULL;

// Rotary encoder
RotaryEncoder *encoder = NULL;
int last_rotary = 0;

// SD Card state
bool sdCardReady = false;
std::vector<String> currentList;
int scrollOffset = 0;

// Navigation state
String currentArtist = "";
String currentAlbum = "";
String currentPath = "/";
BrowseLevel currentBrowseLevel = LEVEL_ARTISTS;

// Audio playback (no Audio object)
bool isPlaying = false;
String currentSongPath = "";
String currentSongName = "";
std::vector<String> playQueue;
int currentQueueIndex = 0;

// Bluetooth state
bool bluetoothEnabled = false;
bool bluetoothConnected = false;
String bluetoothDeviceName = "";
std::vector<String> bluetoothDevices;
int selectedBluetoothDevice = 0;

// State
int menuItem = 1;
int selectedItemIndex = 0;
PageEnum currentPage = PAGE_MAIN;
PageEnum returnToPage = PAGE_MAIN;

// Button debouncing
unsigned long lastEncoderPress = 0;
unsigned long lastPrevPress = 0;
unsigned long lastNextPress = 0;
unsigned long lastPlayPress = 0;
unsigned long lastRecPress = 0;
unsigned long lastReturnPress = 0;

// Button state tracking
bool lastEncoderState = HIGH;
