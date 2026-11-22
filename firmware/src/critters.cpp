#include "critters.h"
#include <math.h>

// Layout: the creature lives on the left (x 0..11); its scene props sit right of the
// hidden gap column (x 17..32), so nothing important ever lands on the join.

namespace Critters {
namespace {

uint32_t hash(uint32_t x) { x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16; return x; }

// ASCII art -> pixels. '#' = on. y may be negative (clipped).
template <int N>
void art(Canvas &c, int x, int y, const char *const (&rows)[N]) {
  for (int r = 0; r < N; r++)
    for (int i = 0; rows[r][i]; i++)
      if (rows[r][i] == '#') c.set(x + i, y + r);
}
void row(Canvas &c, int x, int y, const char *s) { for (int i = 0; s[i]; i++) c.set(x + i, y, s[i] == '#'); }

// ---------------- shared scenery ----------------
void sun(Canvas &c, uint32_t t) {
  // rises out of the horizon over 6 s, then sits there with turning rays
  float rise = t < 6000 ? t / 6000.f : 1;
  int cx = 27, cy = 7 - (int)lroundf(rise * 4);
  for (int dx = -2; dx <= 2; dx++)
    for (int dy = -2; dy <= 2; dy++)
      if (dx * dx + dy * dy <= 5 && cy + dy < 7) c.set(cx + dx, cy + dy);
  if (rise >= 1) {
    bool diag = (t / 600) % 2;
    const int8_t ORTHO[4][2] = {{0, -4}, {4, 0}, {-4, 0}, {0, 4}}, DIAG[4][2] = {{3, -3}, {-3, -3}, {3, 3}, {-3, 3}};
    for (auto &d : diag ? DIAG : ORTHO) if (cy + d[1] < 7) c.set(cx + d[0], cy + d[1]);
  }
  for (int x = 18; x < 33; x += 2) c.set(x, 7);   // horizon
}

const char *const MOON[] = {".##", "##.", "#..", "##.", ".##"};   // crescent
void moon(Canvas &c, int cx, int cy) { art(c, cx - 1, cy - 2, MOON); }

void stars(Canvas &c, uint32_t t, int n, uint32_t seed) {
  static const int8_t POS[][2] = {{19, 1}, {23, 5}, {31, 4}, {21, 3}, {29, 6}, {18, 6}, {32, 1}};
  for (int i = 0; i < n && i < 7; i++) {
    uint32_t slot = (t / 700 + i * 3) / 4;               // each star re-rolls every ~2.8 s
    if (hash(seed + i * 101 + slot) % 5) c.set(POS[i][0], POS[i][1]);   // on 80% of the time
  }
}

void zz(Canvas &c, int x, int y, uint32_t t) {   // two little z's floating up
  for (int k = 0; k < 2; k++) {
    uint32_t ph = (t + k * 1300) % 2600;
    int zy = y - (int)(ph * 5 / 2600), zx = x + k * 3 + (int)(ph / 900);
    if (zy < -2) continue;
    c.hline(zx, zx + 2, zy); c.set(zx + 1, zy + 1); c.hline(zx, zx + 2, zy + 2);
  }
}

// ---------------- cat ----------------
const char *const CAT[] = {
  "#.....#",
  "##...##",
  "#######",
  "##.#.##",   // eyes
  "#######",
  ".#####.",
  "#######",
  "#######",
};
const char *const CAT_LOAF[] = {   // curled up asleep
  "#.#.......",
  "####......",
  "#..######.",
  "##########",
  "##########",
};

void catTail(Canvas &c, int x, uint32_t t) {
  bool flick = (t % 3000) < 300;
  if (flick) { c.set(x + 9, 1); c.set(x + 8, 2); c.set(x + 8, 3); c.set(x + 8, 4); c.set(x + 7, 5); }
  else { c.set(x + 8, 1); c.set(x + 8, 2); c.set(x + 8, 3); c.set(x + 8, 4); c.set(x + 7, 5); }
}
void catEyes(Canvas &c, int x, int y, bool open) { if (!open) row(c, x, y + 3, "#######"); }
bool blinking(uint32_t t, uint32_t every) { return (t % every) < 160; }

void moth(Canvas &c, int x, int y, uint32_t t) {
  if ((t / 120) % 2) { c.set(x - 1, y); c.set(x + 1, y); }
  else { c.set(x - 1, y + 1); c.set(x + 1, y + 1); }
  c.set(x, y + 1);
}

void catScene(Canvas &c, Period p, uint32_t t) {
  const int X = 1;
  switch (p) {
    case Period::Morning: {
      sun(c, t);
      art(c, X, 0, CAT);
      catTail(c, X, t);
      uint32_t y = t % 6000;
      bool yawn = y > 3500 && y < 4300;
      catEyes(c, X, 0, !yawn && !blinking(t, 4100));
      if (yawn) { c.set(X + 3, 4, false); c.set(X + 3, 5, false); }   // open mouth
      break;
    }
    case Period::Day: {
      // a moth drifts in from the right; the cat watches, swipes, the moth escapes
      uint32_t ph = t % 9000;
      art(c, X, 0, CAT);
      catTail(c, X, t);
      catEyes(c, X, 0, !blinking(t, 3700));
      if (ph < 6000) {
        float q = ph / 6000.f;
        int mx = 31 - (int)lroundf(q * 19), my = 2 + (int)lroundf(sinf(ph * 0.004f) * 2);
        moth(c, mx, my, t);
        if (q > 0.7f) c.set(X + 6, 3, false);            // eyes slide right: watching it
      } else if (ph < 6350) {
        c.set(X + 7, 4); c.set(X + 8, 3); c.set(X + 9, 3);  // paw swipe
        moth(c, 13 + (int)((ph - 6000) / 60), 1 - (int)((ph - 6000) / 120), t);
      } else if (ph < 7600) {
        float q = (ph - 6350) / 1250.f;
        moth(c, 19 + (int)(q * 14), -1 + (int)lroundf(sinf(q * 9) * 1.5f), t);
      }
      break;
    }
    case Period::Evening: {
      moon(c, 28, 2);
      stars(c, t, 3, 7);
      art(c, X, 0, CAT);
      uint32_t g = t % 5000;
      bool groom = g > 2500 && g < 3900;
      catEyes(c, X, 0, !groom && !blinking(t, 4300));
      if (groom) {   // paw up to the face, licking
        int lift = ((g / 200) % 2) ? 0 : 1;
        c.set(X + 6, 4 - lift); c.set(X + 7, 4 - lift); c.set(X + 7, 5 - lift);
        c.set(X + 7, 5, true); c.set(X + 8, 6); c.set(X + 8, 7);
      } else catTail(c, X, t + 1500);
      break;
    }
    case Period::Night: {
      moon(c, 29, 2);
      stars(c, t, 5, 3);
      art(c, X, 3, CAT_LOAF);
      bool inhale = (t % 3200) < 1600;               // breathing: the back rises and falls
      if (inhale) c.hline(X + 5, X + 8, 4);
      zz(c, X + 5, 2, t);
      break;
    }
  }
}

// ---------------- frog ----------------
const char *const FROG_EYES_OPEN[] = {
  ".##.....##.",
  "#..#...#..#",
  "#.##...##.#",
};
const char *const FROG_EYES_SHUT[] = {
  "...........",
  ".##.....##.",
  "####...####",
};
const char *const FROG_BODY[] = {
  ".#########.",
  "#.#.....#.#",
  "#..#####..#",   // smile
  ".#########.",
  "##.#...#.##",   // feet
};

const char *const NOTE[] = {".##", ".#.", "##.", "##."};

void frog(Canvas &c, int x, int y, bool eyesOpen) {
  if (eyesOpen) art(c, x, y, FROG_EYES_OPEN); else art(c, x, y, FROG_EYES_SHUT);
  art(c, x, y + 3, FROG_BODY);
}

// the fly wanders around the right half on a smooth looping path
void flyAt(uint32_t t, int &x, int &y) {
  float s = t * 0.001f;
  x = 23 + (int)lroundf(sinf(s * 1.3f) * 6 + sinf(s * 3.1f) * 2.5f);
  y = 3 + (int)lroundf(sinf(s * 1.9f + 1) * 2 + sinf(s * 5.3f) * 0.8f);
}

void line(Canvas &c, int x0, int y0, int x1, int y1) {   // Bresenham
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1, e = dx + dy;
  for (;;) {
    c.set(x0, y0);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * e;
    if (e2 >= dy) { e += dy; x0 += sx; }
    if (e2 <= dx) { e += dx; y0 += sy; }
  }
}

void frogScene(Canvas &c, Period p, uint32_t t) {
  const int X = 0;
  switch (p) {
    case Period::Morning: {
      sun(c, t);
      uint32_t y = t % 6000;
      bool yawn = y > 3000 && y < 3900;
      frog(c, X, 0, !yawn && !blinking(t, 3900));
      if (yawn) { row(c, X, 4, "#..#####..#"); row(c, X, 5, "#..#...#..#"); }   // big open mouth
      break;
    }
    case Period::Day: {
      // a fly buzzes about; every 7 s the tongue shoots out and gets it
      uint32_t ph = t % 7000;
      int fx, fy;
      if (ph < 4000) {
        flyAt(t, fx, fy);
        frog(c, X, 0, !blinking(t, 2900));
        if ((t / 40) % 3) c.set(fx, fy);                // buzzing flicker
      } else if (ph < 4400) {
        flyAt(t - (ph - 4000), fx, fy);                 // where it was when the tongue left
        float q = ph < 4200 ? (ph - 4000) / 200.f : 1 - (ph - 4200) / 200.f;
        int tx = X + 6 + (int)lroundf((fx - X - 6) * q), ty = 5 + (int)lroundf((fy - 5) * q);
        frog(c, X, 0, true);
        row(c, X, 5, "#..#...#..#");                   // mouth open
        line(c, X + 6, 5, tx, ty);
        if (ph < 4200) c.set(fx, fy); else c.set(tx, ty);
      } else {
        frog(c, X, 0, ph > 4700);                       // gulp: eyes squeeze shut
        if (ph < 4900) row(c, X, 5, "#.#######.#");
      }
      break;
    }
    case Period::Evening: {
      // fireflies glow on and off; the frog croaks with a puffed-up throat
      for (int i = 0; i < 3; i++) {
        float s = t * 0.0006f + i * 2.1f;
        int x = 24 + (int)lroundf(sinf(s) * 6 + i), y = 3 + (int)lroundf(sinf(s * 1.7f + i) * 2.5f);
        uint32_t glow = (t + i * 900) % 2700;
        if (glow < 1100) c.set(x, y);
      }
      uint32_t k = t % 4000;
      bool croak = k > 2600 && k < 3400;
      frog(c, X, 0, !blinking(t, 3300));
      if (croak) {   // "ribbit": mouth pops open twice and a note floats up
        if ((k / 200) % 2 == 0) row(c, X, 5, "#..#...#..#");
        float q = (k - 2600) / 800.f;
        int nx = X + 12 + (int)(q * 3), ny = 3 - (int)lroundf(q * 4);
        art(c, nx, ny, NOTE);
      }
      break;
    }
    case Period::Night: {
      moon(c, 29, 2);
      stars(c, t, 4, 11);
      frog(c, X, -1, false);                            // asleep on a lily pad
      row(c, X, 7, "#####.######");
      for (int x = 14; x < 33; x++)                      // slow ripples
        if ((x + t / 400) % 7 == 0) c.set(x, 7);
      zz(c, X + 11, 1, t);
      break;
    }
  }
}
}  // namespace

Period periodFor(int m) {
  if (m < 0) return Period::Night;
  if (m >= 330 && m < 540) return Period::Morning;    // 5:30 - 9:00
  if (m >= 540 && m < 1050) return Period::Day;       // 9:00 - 17:30
  if (m >= 1050 && m < 1260) return Period::Evening;  // 17:30 - 21:00
  return Period::Night;
}
const char *periodName(Period p) {
  switch (p) { case Period::Morning: return "morning"; case Period::Day: return "day"; case Period::Evening: return "evening"; default: return "night"; }
}

void draw(Canvas &c, Kind k, Period p, uint32_t t) {
  if (k == Kind::Cat) catScene(c, p, t); else frogScene(c, p, t);
}

void drawHop(Canvas &c, Kind k, float p) {
  int y = -(int)lroundf(sinf(p * 3.14159f) * 3);
  if (k == Kind::Cat) art(c, 1, y, CAT);
  else frog(c, 0, y, true);
  if (p > 0.85f) { c.set(k == Kind::Cat ? 0 : -1, 7); c.set(k == Kind::Cat ? 9 : 11, 7); }   // landing puff
}
}  // namespace Critters
