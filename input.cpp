#include "input.h"
#include "globals.h"
#include "config.h"
#include <Arduino.h>
#include <RotaryEncoder.h>

// audio_player.h removed — playback handled by playback.cpp

static bool lastStateReturn = HIGH;
static bool lastStateRec    = HIGH;
static bool lastStateNext   = HIGH;
static bool lastStatePrev   = HIGH;
static bool lastStatePlay   = HIGH;

void IRAM_ATTR checkPosition() {
    if (encoder != NULL) encoder->tick();
}

bool encoderPressed() {
    bool currentState = digitalRead(BUTTON_IN);
    if (lastEncoderState == HIGH && currentState == LOW) {
        unsigned long now = millis();
        if (now - lastEncoderPress > debounceDelay) {
            lastEncoderPress = now;
            lastEncoderState = currentState;
            return true;
        }
    }
    lastEncoderState = currentState;
    return false;
}

bool prevPressed() {
    bool currentState = digitalRead(BTN_PREV);
    if (lastStatePrev == HIGH && currentState == HIGH) {
        unsigned long now = millis();
        if (now - lastPrevPress > debounceDelay) {
            lastPrevPress = now;
            lastStatePrev = currentState;
            return true;
        }
    }
    lastStatePrev = currentState;
    return false;
}

bool nextPressed() {
    bool currentState = digitalRead(BTN_NEXT);
    if (lastStateNext == HIGH && currentState == HIGH) {
        unsigned long now = millis();
        if (now - lastNextPress > debounceDelay) {
            lastNextPress = now;
            lastStateNext = currentState;
            return true;
        }
    }
    lastStateNext = currentState;
    return false;
}

bool playPressed() {
    bool currentState = digitalRead(BTN_PLAY);
    if (lastStatePlay == HIGH && currentState == HIGH) {
        unsigned long now = millis();
        if (now - lastPlayPress > debounceDelay) {
            lastPlayPress = now;
            lastStatePlay = currentState;
            return true;
        }
    }
    lastStatePlay = currentState;
    return false;
}

bool recPressed() {
    bool currentState = digitalRead(BTN_REC);
    if (lastStateRec == HIGH && currentState == HIGH) {
        unsigned long now = millis();
        if (now - lastRecPress > debounceDelay) {
            lastRecPress = now;
            lastStateRec = currentState;
            return true;
        }
    }
    lastStateRec = currentState;
    return false;
}

bool returnPressed() {
    bool currentState = digitalRead(BTN_RETURN);
    if (lastStateReturn == HIGH && currentState == LOW) {
        unsigned long now = millis();
        if (now - lastReturnPress > debounceDelay) {
            lastReturnPress = now;
            lastStateReturn = currentState;
            return true;
        }
    }
    lastStateReturn = currentState;
    return false;
}

int getEncoderChange() {
    if (encoder == NULL) return 0;
    int curr_rotary = encoder->getPosition();
    int change = 0;
    if (curr_rotary != last_rotary) {
        change = (curr_rotary > last_rotary) ? -1 : 1;
        last_rotary = curr_rotary;
    }
    return change;
}
