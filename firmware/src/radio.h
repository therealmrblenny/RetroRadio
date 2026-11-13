#pragma once
#include <Arduino.h>

// Owns the Audio object. All audio calls happen on the radio task; other code sends commands.
namespace Radio {
struct Snapshot {
  char name[96];      // our station name
  char title[192];    // ICY stream title (artist - song)
  char codec[16];
  uint32_t bitrate;
  bool wantPlaying;   // we asked it to play
  bool running;       // decoder is actually running
  uint32_t sinceMs;   // millis() of last play request
};
void begin();
void play(const char *name, const char *url);
void stop();
void setVolume(uint8_t v);  // 0..VOL_MAX
Snapshot snapshot();
}
