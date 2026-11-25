#pragma once
#include <Arduino.h>
#include <vector>

// ── Playback mode ─────────────────────────────────────────────────────────────
enum PlaybackMode {
    PLAYBACK_NONE,
    PLAYBACK_BLUETOOTH,
    PLAYBACK_I2S        // wired DAC — enabled when DAC is wired up
};

// ── Queue entry ───────────────────────────────────────────────────────────────
struct QueueEntry {
    String title;
    String path;
};

// ── Public API ────────────────────────────────────────────────────────────────

// Call once from setup() — safe, no hardware init yet
void playback_init();

// Call every loop() iteration
void playback_loop();

// Load a list of tracks into the play queue and start playing from startIndex
void playback_startQueue(const std::vector<QueueEntry>& tracks, int startIndex = 0);

// Control
void playback_next();
void playback_previous();
void playback_togglePause();
void playback_stop();

// State queries
bool playback_isPlaying();
bool playback_isPaused();
PlaybackMode playback_currentMode();
int playback_queueIndex();
int playback_queueSize();
String playback_currentTitle();
String playback_currentPath();

// Called automatically by playback_loop() but exposed so BT menu can call it
void playback_enableBluetooth();
void playback_disableBluetooth();
bool playback_isBluetoothEnabled();
bool playback_isBluetoothConnected();

// Bluetooth device management
void playback_btScan();
void playback_btConnect(int deviceIndex);  // connect to scanned device by index

// I2S pin config — call before first playback if using I2S
// Default pins chosen to avoid SD/OLED conflicts: BCLK=14, LRC=21, DIN=22
void playback_setI2SPins(int bclk, int lrc, int din);
