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
  uint32_t bufFill, bufSize;   // audio input buffer (bytes)
  uint32_t bufMin;             // lowest fill in the last ~2s
  uint32_t gapMaxUs;           // longest gap between audio.loop() calls in the last ~2s
  uint32_t loopMaxUs;          // longest single audio.loop() in the last ~2s
};
void begin();
void play(const char *name, const char *url);
void stop();
void setVolume(uint8_t v);  // 0..VOL_MAX
Snapshot snapshot();
}
