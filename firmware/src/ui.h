#pragma once
#include <Arduino.h>
#include "config.h"

// Controller: owns the radio's user-facing state (volume, station, standby, browse)
// and reacts to Input actions. What it shows is drawn by View (view.h).
namespace UI {
enum class Mode : uint8_t { Boot, Connecting, SetupAP, Notice, Playing, Browse, Lists, Standby };
void begin();
void loop();                     // call often from loop()
void frame(uint8_t out[MTX_W]);  // copy of the current frame (for the web marquee)
Mode mode();
const char *modeName();
int volume();
int listIndex();
int stationIndex();
int brightness();
bool compactFont();
void setTest(int pattern);       // wiring test pattern, 0 = off
void tryStation(const String &name, const String &url);  // play a URL that isn't in a list (any task)
}
