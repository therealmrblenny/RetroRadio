#pragma once
#include <Arduino.h>
// Driver for 2x Jaycar XC3746 (AIP1640, 16x8 each) sharing one clock line.
namespace Matrix {
void begin();
void setBrightness(uint8_t level);   // 0..7
void setOn(bool on);
void show(const uint8_t *cols);      // MTX_W logical columns (gap column dropped), bit0 = top row
uint32_t lastPushUs();               // how long the last frame took to send (bit-banged)
}
