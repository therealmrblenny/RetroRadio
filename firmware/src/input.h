#pragma once
#include <Arduino.h>
// Everything that can drive the UI: the knob, the web page, or serial test keys.
enum class Act : uint8_t {
  TurnCW, TurnCCW, Press, LongPress,   // knob (or simulated knob)
  SetVolume, Play, Next, Prev, Standby, // direct commands (web)
  SetBrightness, StationsChanged,
};
struct Action { Act type; int32_t v; };

namespace Input {
void begin();
void poll();                 // call from loop: converts encoder/button state into actions
void post(Act t, int32_t v = 0);  // thread-safe, from any task
bool next(Action &a);        // pop next action (UI loop)
}
