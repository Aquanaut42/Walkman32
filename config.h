#ifndef CONFIG_H
#define CONFIG_H

//OLED pins
#define OLED_RST 22
#define OLED_DC  19
#define OLED_CS  23
#define OLED_CLK 18
#define OLED_DIN 5

// Rotary Encoder pins
#define PIN_ENCODER_A 17
#define PIN_ENCODER_B 16
#define BUTTON_IN     27

// Control buttons
#define BTN_RETURN 2
#define BTN_REC    13
#define BTN_PLAY   15
#define BTN_NEXT   14
#define BTN_PREV   4

// SD card pins
#define SD_SCK     32 //CLK
#define SD_MISO    33 //SO
#define SD_MOSI    25 //SI
#define SD_CS      26 //CS

// Battery monitor: single Li-ion cell -> 47k / 47k divider -> GPIO34 (ADC1, input-only, no pull-up)
#define BATTERY_ADC_PIN 34
const float         BATTERY_DIVIDER_RATIO = 2.0f;   // (R_top + R_bottom) / R_bottom
const float         BATTERY_CALIBRATION   = 1.0f;   // real voltage / reported voltage (tune with a multimeter)
const unsigned long BATTERY_SAMPLE_MS     = 100;    // one ADC conversion per visit, so audio is never stalled
const int           BATTERY_SAMPLES       = 8;      // conversions averaged per reading

// Display settings
const int MAX_VISIBLE_ITEMS = 9;

// Audio formats
// MP3, WAV and FLAC are built in. AAC (.aac / .m4a) additionally needs the Helix AAC decoder from the
// "libhelix" Arduino library; it is enabled automatically when that library is found.
// To force it on or off, uncomment one of these:
//#define FW_ENABLE_AAC 1    // on  (install "libhelix" via the Library Manager first)
//#define FW_ENABLE_AAC 0    // off (.aac / .m4a tracks are skipped)

// Debounce settings
const unsigned long debounceDelay = 200;

#endif
