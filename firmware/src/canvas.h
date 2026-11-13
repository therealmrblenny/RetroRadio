#pragma once
#include <Arduino.h>
#include "config.h"

// 32x8 monochrome frame. col[x] bit y = pixel (x,y), bit0 = top row.
struct Canvas {
  uint8_t col[MTX_W];
  void clear() { memset(col, 0, sizeof(col)); }
  void set(int x, int y, bool on = true) {
    if (x < 0 || x >= MTX_W || y < 0 || y >= MTX_H) return;
    if (on) col[x] |= (1 << y); else col[x] &= ~(1 << y);
  }
  void column(int x, uint8_t bits) { if (x >= 0 && x < MTX_W) col[x] |= bits; }
  // Draw proportional text starting at x (may be negative). Returns width in pixels.
  int text(int x, const char *utf8, int yOffset = 0);
  static int textWidth(const char *utf8);
};
