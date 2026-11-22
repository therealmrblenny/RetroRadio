#pragma once
// Everything that appears on the 32x8 display: scenes, transitions and animations.
// Pure C++ with no Arduino dependencies, so the PC simulator (sim/) runs the exact
// same code to preview animations. The UI controller (ui.cpp) decides WHAT to show
// by calling the show*() functions; this module decides HOW it looks and moves.
// All times are millis(). render() is called once per frame (FRAME_MS).
#include <stdint.h>
#include <string>
#include <vector>
#include "canvas.h"

namespace View {
constexpr uint32_t FRAME_MS = 40;   // 25 fps: marquee text moves exactly 1 px per frame

enum class Icon : uint8_t { None, Wifi, Folder, Back };
struct Page { std::string text; Icon icon = Icon::None; };

void setFont(Font f);
Font font();
void setUserBrightness(int b);      // 0..7, used for fades

// ---- scenes ----
void showBoot(uint32_t now);                      // light sweep + RETRO / RADIO slam
bool bootDone(uint32_t now);
void showConnecting(uint32_t now);                // pulsing wifi icon
// Text pages, each held or scrolled once, sliding up to the next. loop = repeat forever.
void showPages(uint32_t now, const std::vector<Page> &pages, bool loop);
bool pagesDone(uint32_t now);
// Station change: static burst, tuning needle sweeps to pos/count, name drops in.
// Called while waking, it plays after the wake-up animation.
void tuneTo(uint32_t now, const char *name, int pos, int count);
void showPlaying(uint32_t now);                   // back to now-playing without the tuning effect
void setNowPlaying(const char *name, const char *title, bool buffering, bool stalled);
void showVolume(uint32_t now, int vol, int max);  // overlay over whatever is showing
void hideVolume();
// Browse reel. dir: +1 slides up (turned clockwise), -1 slides down, 0 = cut in from playing.
void showBrowse(uint32_t now, const char *label, int pos, int count, int dir, Icon icon = Icon::None);
void setClock(int minuteOfDay);                   // local time for the standby scenes, -1 = unknown
// Standby: a cat or frog (seed picks) in a scene for the time of day; the display fades down.
void showStandby(uint32_t now, uint32_t seed);
void wake(uint32_t now);                          // the creature hops (then the queued tuneTo)
bool busy(uint32_t now);                          // an intro animation is still running

// How `text` looks when the playing screen settles on it (for the web preview).
// Returns its width in pixels; wider than the screen means it is cut off.
int preview(const char *text, Canvas &out);

// Draw the frame for `now`. Returns a brightness override (0..7) or -1 for the user setting.
int render(uint32_t now, Canvas &out);
}  // namespace View
