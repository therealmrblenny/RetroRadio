#pragma once
// Pure C++ (no Arduino deps) so it also builds in the PC simulator (sim/).
#include <stdint.h>
#include <string.h>
#include "config.h"

enum class Font : uint8_t {
  Classic,   // proportional 5x7 with lowercase (~6 px per char)
  Compact,   // condensed caps-only (~4 px per char: about 8 characters fit)
};

// 32x8 monochrome frame. col[x] bit y = pixel (x,y), bit0 = top row.
struct Canvas {
  uint8_t col[MTX_W];
  void clear() { memset(col, 0, sizeof(col)); }
  bool get(int x, int y) const { return x >= 0 && x < MTX_W && y >= 0 && y < MTX_H && (col[x] >> y & 1); }
  void set(int x, int y, bool on = true) {
    if (x < 0 || x >= MTX_W || y < 0 || y >= MTX_H) return;
    if (on) col[x] |= (1 << y); else col[x] &= ~(1 << y);
  }
  // OR a column bit pattern in; y > 0 shifts it down, y < 0 up
  void column(int x, uint8_t bits, int y = 0) {
    if (x < 0 || x >= MTX_W) return;
    col[x] |= y >= 0 ? (uint8_t)(bits << y) : (uint8_t)(bits >> -y);
  }
  void vline(int x, int y0, int y1) { for (int y = y0; y <= y1; y++) set(x, y); }
  void hline(int x0, int x1, int y) { for (int x = x0; x <= x1; x++) set(x, y); }
  // Draw text starting at x (may be negative). Returns width in pixels.
  int text(int x, const char *utf8, int y = 0, Font f = Font::Classic);
  static int textWidth(const char *utf8, Font f = Font::Classic);
};
