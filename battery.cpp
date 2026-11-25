// battery.cpp — see battery.h

#include "battery.h"
#include "config.h"

static uint32_t s_adcMv      = 0;      // averaged ADC pin voltage
static uint32_t s_batteryMv  = 0;      // filtered battery voltage
static float    s_filteredMv = 0;
static int      s_percent    = -1;
static bool     s_changed    = false;

int battery_estimatePercent(float millivolts) {
    // Approximate UI profile for a conventional 3.7 V nominal / 4.2 V full cell.
    // Tune against your cell's discharge curve. Not for LiFePO4 or multi-cell packs.
    // The 0% display point is NOT a hardware undervoltage cutoff.
    static const uint16_t voltage[] = {3300, 3500, 3600, 3700, 3750, 3800,
                                       3850, 3900, 4000, 4100, 4200};
    static const uint8_t  percent[] = {0, 5, 10, 20, 35, 50, 60, 70, 85, 95, 100};
    if (millivolts <= voltage[0]) return 0;
    for (size_t i = 1; i < sizeof(voltage) / sizeof(voltage[0]); ++i) {
        if (millivolts <= voltage[i]) {
            const float fraction = (millivolts - voltage[i - 1]) / float(voltage[i] - voltage[i - 1]);
            return int(percent[i - 1] + fraction * (percent[i] - percent[i - 1]) + 0.5f);
        }
    }
    return 100;
}

void battery_init() {
    pinMode(BATTERY_ADC_PIN, INPUT);
    analogReadResolution(12);
    analogSetPinAttenuation(BATTERY_ADC_PIN, ADC_11db);   // full 0..3.1 V range for the divider
    battery_update(true);
    s_changed = false;                                     // the first reading is not a "change"
}

void battery_update(bool immediate) {
    static uint32_t lastSample = 0, totalMv = 0;
    static uint8_t  sampleCount = 0;
    const uint32_t now = millis();
    if (!immediate && now - lastSample < BATTERY_SAMPLE_MS) return;
    lastSample = now;

    // One conversion per visit: never pause the audio pipeline for a burst of samples.
    totalMv += analogReadMilliVolts(BATTERY_ADC_PIN);
    ++sampleCount;
    if (!immediate && sampleCount < BATTERY_SAMPLES) return;

    s_adcMv = (totalMv + sampleCount / 2) / sampleCount;
    totalMv = 0;
    sampleCount = 0;

    const float measured = s_adcMv * BATTERY_DIVIDER_RATIO * BATTERY_CALIBRATION;
    const int previous = s_percent;

    // With the bottom resistor fitted, a disconnected sense supply reads near 0 V.
    // Reject an absent / implausible input instead of showing a fabricated percentage.
    if (measured < 2500 || measured > 4350) {
        s_filteredMv = 0;
        s_batteryMv  = 0;
        s_percent    = -1;
    } else {
        s_filteredMv = s_filteredMv == 0 ? measured : s_filteredMv + 0.15f * (measured - s_filteredMv);
        s_batteryMv  = uint32_t(s_filteredMv + 0.5f);
        const int estimate = battery_estimatePercent(s_filteredMv);
        // Two percentage points of hysteresis suppress flicker from playback load.
        if (s_percent < 0 || abs(estimate - s_percent) >= 2 || estimate == 0 || estimate == 100)
            s_percent = estimate;
    }
    if (previous != s_percent) s_changed = true;
}

int      battery_percent()    { return s_percent; }
uint32_t battery_millivolts() { return s_batteryMv; }

bool battery_takeChanged() {
    bool c = s_changed;
    s_changed = false;
    return c;
}

void battery_printStatus() {
    Serial.printf("Battery ADC: %lu mV | filtered battery: %lu mV | estimate: ",
                  (unsigned long)s_adcMv, (unsigned long)s_batteryMv);
    if (s_percent < 0) Serial.println("unknown; check divider and sense supply");
    else Serial.printf("%d%% (voltage estimate)\n", s_percent);
}
