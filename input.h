#ifndef INPUT_H
#define INPUT_H

#include <Arduino.h>

void IRAM_ATTR checkPosition();
bool encoderPressed();
bool prevPressed();
bool nextPressed();
bool playPressed();
bool recPressed();
bool returnPressed();
int getEncoderChange();

#endif
