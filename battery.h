// battery.h — battery level from a voltage divider on an ADC pin
// Ported from the ESP32_SD_BT_Player project (same voltage curve, filtering and hysteresis).
//
//   battery_init();            once in setup(), BEFORE audio starts
//   battery_update();          every loop(); takes at most one ADC conversion per call, never blocks
//   battery_percent();         0..100, or -1 when the sense input is absent / implausible
//   battery_takeChanged();     true exactly once after the displayed percentage changed
#pragma once
#include <Arduino.h>

void     battery_init();
void     battery_update(bool immediate = false);
int      battery_percent();
uint32_t battery_millivolts();     // filtered battery voltage, 0 when unknown
bool     battery_takeChanged();
void     battery_printStatus();    // Serial: raw ADC mV, battery mV, percentage

// Exposed for testing: maps a battery voltage (mV) to a UI percentage (conventional 3.7 V / 4.2 V cell).
int      battery_estimatePercent(float millivolts);
