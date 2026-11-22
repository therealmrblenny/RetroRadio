#include "canvas.h"
#include "font5x7.h"
#include "font_caps.h"

namespace {
// Latin-1 0xC0..0xFF folded to plain ASCII so accented titles still render
const char LATIN1[] = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYPsaaaaaaaceeeeiiiidnooooo/ouuuuypy";

// Decode the next UTF-8 char and map it to a font index char (0x20..0x7F)
char nextChar(const char *&p) {
  uint8_t c = (uint8_t)*p++;
  if (c < 0x80) return (c >= 0x20) ? (char)c : ' ';
  uint32_t cp; int extra;
  if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }
  else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
  else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
  else return '?';
  while (extra-- && (*p & 0xC0) == 0x80) cp = (cp << 6) | (*p++ & 0x3F);
  if (cp >= 0xC0 && cp <= 0xFF) return LATIN1[cp - 0xC0];
  switch (cp) {
    case 0xB7: case 0x2022: case 0x25C6: return 0x7F;   // middle dot, bullet, diamond
    case 0x2018: case 0x2019: case 0xB4: return '\'';
    case 0x201C: case 0x201D: return '"';
    case 0x2013: case 0x2014: return '-';
    case 0x2026: return '.';
    case 0xA0: return ' ';
  }
  return '?';
}

// Columns of one glyph: pointer + count (proportional spacing)
const uint8_t *glyph(char ch, Font f, int &w) {
  if (f == Font::Compact) {
    const uint16_t *e = FONT_CAPS_INDEX[ch - 0x20];
    w = e[1];
    return FONT_CAPS_COLS + e[0];
  }
  static const uint8_t SPACE[2] = {0, 0};
  if (ch == ' ') { w = 2; return SPACE; }
  const uint8_t *g = FONT5X7[ch - 0x20];
  int first = 0, last = 4;
  while (first < 4 && !g[first]) first++;
  while (last > first && !g[last]) last--;
  w = last - first + 1;
  return g + first;
}
}  // namespace

int Canvas::textWidth(const char *s, Font f) {
  int w = 0;
  const char *p = s;
  while (*p) { int gw; glyph(nextChar(p), f, gw); w += gw + 1; }
  return w ? w - 1 : 0;
}

int Canvas::text(int x, const char *s, int y, Font f) {
  int start = x;
  const char *p = s;
  while (*p) {
    int gw;
    const uint8_t *g = glyph(nextChar(p), f, gw);
    if (x > MTX_W) { x += gw + 1; continue; }   // off the right edge: just measure
    for (int i = 0; i < gw; i++, x++) column(x, g[i], y);
    x++;  // 1px gap
  }
  return x - start - 1;
}
