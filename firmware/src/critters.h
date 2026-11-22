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
// t = ms since standby started (drives the scene's loops)
void draw(Canvas &c, Kind k, Period p, uint32_t t);
// wake-up hop: p = 0..1 through the hop
void drawHop(Canvas &c, Kind k, float p);
}
