#pragma once
#include <Arduino.h>
#include "config.h"

// Screen state machine: renders the 32x8 frame and reacts to Input actions.
namespace UI {
enum class Mode : uint8_t { Boot, Connecting, SetupAP, Playing, Browse, Standby };
void begin();
void loop();                     // call often from loop()
void frame(uint8_t out[MTX_W]);  // copy of the current frame (for the web marquee)
Mode mode();
int volume();
int stationIndex();
int brightness();
}
