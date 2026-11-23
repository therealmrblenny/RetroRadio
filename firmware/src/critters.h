#pragma once
// Standby mascots: a cat and a frog with little scenes that change with the time of day.
// Pure C++ (built by the PC simulator too). Drawn by View during standby.
#include <stdint.h>
#include "canvas.h"

namespace Critters {
enum class Kind : uint8_t { Cat, Frog };
enum class Period : uint8_t { Morning, Day, Evening, Night };

Period periodFor(int minuteOfDay);   // -1 (clock not set yet) counts as night
const char *periodName(Period p);
// Start a standby session: random spot on screen, random schedule (seeded).
void begin(Kind k, uint32_t seed);
// t = ms since standby started. Advances the session (hops, peeking, the main act every
// 5-10 min) and draws it.
void draw(Canvas &c, Period p, uint32_t t);
// wake-up hop from wherever the creature is: p = 0..1 through the hop
void drawHop(Canvas &c, float p);
void poke(uint32_t t);          // do the main act now (web "poke" button)
void setPace(uint32_t div);     // divide all waiting times (simulator demo); 1 = real time
}
