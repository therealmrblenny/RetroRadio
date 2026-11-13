#include "canvas.h"
#include "font5x7.h"

// Latin-1 0xC0..0xFF folded to plain ASCII so accented titles still render
static const char LATIN1[] = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYPsaaaaaaaceeeeiiiidnooooo/ouuuuypy";

// Decode the next UTF-8 char and map it to a font index char (0x20..0x7F)
static char nextChar(const char *&p) {
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
    case 0xB7: case 0x2022: return 0x7F;          // middle dot, bullet
    case 0x2018: case 0x2019: case 0xB4: return '\'';
    case 0x201C: case 0x201D: return '"';
    case 0x2013: case 0x2014: return '-';
    case 0x2026: return '.';
    case 0xA0: return ' ';
  }
  return '?';
}

// Visible column range of a glyph (proportional spacing)
static void glyphSpan(char ch, int &first, int &last) {
  if (ch == ' ') { first = 0; last = 1; return; }  // 2px wide space
  const uint8_t *g = FONT5X7[ch - 0x20];
  first = 0; last = 4;
  while (first < 4 && !g[first]) first++;
  while (last > first && !g[last]) last--;
}

int Canvas::textWidth(const char *s) {
  int w = 0; const char *p = s;
  while (*p) { int f, l; glyphSpan(nextChar(p), f, l); w += (l - f + 1) + 1; }
  return w ? w - 1 : 0;
}

int Canvas::text(int x, const char *s, int yOffset) {
  int start = x; const char *p = s;
  while (*p) {
    char ch = nextChar(p);
    int f, l; glyphSpan(ch, f, l);
    const uint8_t *g = FONT5X7[ch - 0x20];
    for (int i = f; i <= l; i++, x++) {
      if (x >= 0 && x < MTX_W) col[x] |= (yOffset >= 0) ? (g[i] << yOffset) : (g[i] >> -yOffset);
    }
    x++;  // 1px gap
  }
  return x - start - 1;
}
