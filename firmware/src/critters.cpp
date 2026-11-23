#include "critters.h"
#include <math.h>
#include <stdlib.h>

// A standby session is a little life: the creature sits somewhere on the screen (never
// straddling the hidden gap column), now and then hops to a new spot or sinks down so
// only its eyes peek over the bottom edge, and every 5-10 minutes does its "main act"
// (fly, moth, croak, groom...). In between it only blinks, glances and breathes.
// Creature art is drawn in its own coordinates facing right; a Pen mirrors it when it
// sits on the right half so it always faces the open side, where its props appear.

namespace Critters {
namespace {

const float PI_F = 3.14159265f;
uint32_t hash(uint32_t x) { x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16; return x; }
float clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }

struct Pen {
  Canvas &c;
  int ox, oy, w;
  bool flip;
  int gx(int x) const { return flip ? ox + w - 1 - x : ox + x; }   // local -> global
  int lx(int g) const { return flip ? ox + w - 1 - g : g - ox; }   // global -> local
  void set(int x, int y, bool on = true) const { c.set(gx(x), oy + y, on); }
  void hline(int x0, int x1, int y) const { for (int x = x0; x <= x1; x++) set(x, y); }
};

// ASCII art -> pixels. '#' = on, others untouched.
template <int N>
void art(const Pen &p, int x, int y, const char *const (&rows)[N]) {
  for (int r = 0; r < N; r++)
    for (int i = 0; rows[r][i]; i++)
      if (rows[r][i] == '#') p.set(x + i, y + r);
}
// one row, '#' = on and '.' = off (used to redraw eyes/mouths)
void row(const Pen &p, int x, int y, const char *s) { for (int i = 0; s[i]; i++) p.set(x + i, y, s[i] == '#'); }

void line(const Pen &p, int x0, int y0, int x1, int y1) {   // Bresenham, local coords
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1, e = dx + dy;
  for (;;) {
    p.set(x0, y0);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * e;
    if (e2 >= dy) { e += dy; x0 += sx; }
    if (e2 <= dx) { e += dx; y0 += sy; }
  }
}

// ---------------- the life of one standby session ----------------
enum class Mv : uint8_t { None, Hop, Sink, Rise };
const int CAT_W = 10, FROG_W = 11;
const uint32_t HOP_MS = 600, SINK_MS = 500;

struct Life {
  Kind kind = Kind::Cat;
  uint32_t rng = 1, salt = 0;
  int x = 0;
  bool flip = false, peek = false;
  Mv mv = Mv::None;
  uint32_t mvStart = 0, mvDur = 0;
  int fromX = 0, toX = 0, hops = 1;
  bool acting = false;
  uint32_t actStart = 0;
  Period actP = Period::Night;
  uint8_t variant = 0;
  int actOpen = 24;          // centre of the open side when the act began (props live there)
  uint32_t nextAct = 0, nextMove = 0;
  bool skyRight = true;
} L;
uint32_t paceDiv = 1;

uint32_t rnd() { L.rng ^= L.rng << 13; L.rng ^= L.rng >> 17; L.rng ^= L.rng << 5; return L.rng; }
uint32_t rnd(uint32_t lo, uint32_t hi) { return lo + rnd() % (hi - lo + 1); }
uint32_t paced(uint32_t ms) { return ms / paceDiv; }

int width() { return L.kind == Kind::Cat ? CAT_W : FROG_W; }
int peekDepth() { return L.kind == Kind::Cat ? 4 : 5; }   // cat: ears+eyes, frog: just the eyes
bool validSpot(int x) {
  int w = width();
  return x >= 0 && x + w <= MTX_W && !(MTX_GAP_COL >= x && MTX_GAP_COL < x + w);
}
int randomSpot(int avoid) {
  for (int i = 0; i < 64; i++) {
    int x = rnd() % (MTX_W - width() + 1);
    if (validSpot(x) && (avoid < 0 || abs(x - avoid) >= 5)) return x;
  }
  return avoid < 8 && validSpot(MTX_W - width()) ? MTX_W - width() : 0;
}
bool onRight(int x) { return x + width() / 2 > MTX_W / 2; }
int openCentre(int x) { return onRight(x) ? 8 : 24; }

void startMove(Mv m, uint32_t t, int to) {
  L.mv = m; L.mvStart = t; L.fromX = L.x; L.toX = to;
  if (m == Mv::Hop) { L.hops = abs(to - L.x) > 9 ? 2 : 1; L.mvDur = HOP_MS * L.hops; }
  else L.mvDur = SINK_MS;
}

uint32_t actLen(Kind k, Period p, uint8_t v) {
  if (k == Kind::Cat) {
    switch (p) { case Period::Morning: return 2600; case Period::Day: return 7800; case Period::Evening: return 3200; default: return 2000; }
  }
  switch (p) {
    case Period::Morning: return 2200;
    case Period::Day: return 5400;
    case Period::Evening: return 900 * (1 + v % 3) + 200;   // 1-3 ribbits
    default: return 2400;
  }
}

void update(Period p, uint32_t t) {
  if (L.mv != Mv::None && t >= L.mvStart + L.mvDur) {
    if (L.mv == Mv::Hop) { L.x = L.toX; L.flip = onRight(L.x); }
    else L.peek = L.mv == Mv::Sink;
    L.mv = Mv::None;
  }
  if (L.acting && t >= L.actStart + actLen(L.kind, L.actP, L.variant)) {
    L.acting = false;
    if (L.kind == Kind::Cat && L.actP == Period::Night) L.flip = !L.flip;   // rolled over in its sleep
    L.nextAct = t + paced(rnd(300000, 600000));                             // next act in 5-10 min
    if (L.nextMove < t + paced(20000)) L.nextMove = t + paced(rnd(20000, 60000));
  }
  if (L.acting || L.mv != Mv::None) return;
  bool night = p == Period::Night;
  if (night) L.peek = false;                       // asleep: lying down, not peeking
  if (t >= L.nextAct) {
    if (L.peek) startMove(Mv::Rise, t, L.x);       // pop up first, then act
    else {
      L.acting = true; L.actStart = t; L.actP = p; L.variant = rnd() & 0xFF;
      L.actOpen = openCentre(L.x);
    }
    return;
  }
  if (!night && t >= L.nextMove) {
    if (L.peek) startMove(Mv::Rise, t, L.x);
    else if (rnd() % 100 < 65) startMove(Mv::Hop, t, randomSpot(L.x));
    else startMove(Mv::Sink, t, L.x);
    L.nextMove = t + paced(rnd(90000, 240000));    // 1.5-4 min between moves
  }
}

// little bits of idle life, pseudo-random per time slot
bool blinking(uint32_t t, uint32_t salt) {
  uint32_t slot = t / 4000, ph = t % 4000, h = hash(slot * 31 + salt), at = 300 + h % 3200;
  if (ph >= at && ph < at + 150) return true;
  return (h >> 12) % 5 == 0 && ph >= at + 300 && ph < at + 450;   // sometimes a double blink
}
int glance(uint32_t t, uint32_t salt) {   // -1 / 0 / +1 (+1 = towards the open side)
  uint32_t slot = t / 6000, h = hash(slot * 131 + salt + 7);
  if (h % 3) return 0;
  uint32_t ph = t % 6000, at = (h >> 8) & 2047;
  if (ph < at || ph > at + 1600) return 0;
  return (h >> 20) & 1 ? 1 : -1;
}

// ---------------- scenery (global coordinates) ----------------
void sun(Canvas &c, int cx, uint32_t t) {
  // rises out of the horizon over 6 s, then sits there with turning rays
  float rise = t < 6000 ? t / 6000.f : 1;
  int cy = 7 - (int)lroundf(rise * 4);
  for (int dx = -2; dx <= 2; dx++)
    for (int dy = -2; dy <= 2; dy++)
      if (dx * dx + dy * dy <= 5 && cy + dy < 7) c.set(cx + dx, cy + dy);
  if (rise >= 1) {
    bool diag = (t / 600) % 2;
    const int8_t ORTHO[4][2] = {{0, -4}, {4, 0}, {-4, 0}, {0, 4}}, DIAG[4][2] = {{3, -3}, {-3, -3}, {3, 3}, {-3, 3}};
    for (auto &d : diag ? DIAG : ORTHO) if (cy + d[1] < 7) c.set(cx + d[0], cy + d[1]);
  }
  for (int x = 0; x < MTX_W; x += 2) c.set(x, 7);   // horizon
}

const char *const MOON[] = {".##", "##.", "#..", "##.", ".##"};   // crescent
void moon(Canvas &c, int cx, int cy) {
  for (int r = 0; r < 5; r++)
    for (int i = 0; i < 3; i++)
      if (MOON[r][i] == '#') c.set(cx - 1 + i, cy - 2 + r);
}

void stars(Canvas &c, uint32_t t, int n, uint32_t seed, bool right) {
  static const int8_t POS[][2] = {{19, 1}, {23, 5}, {31, 4}, {21, 3}, {29, 6}, {18, 6}, {32, 1}};
  for (int i = 0; i < n && i < 7; i++) {
    uint32_t slot = (t / 700 + i * 3) / 4;               // each star re-rolls every ~2.8 s
    if (hash(seed + i * 101 + slot) % 5) c.set(right ? POS[i][0] : POS[i][0] - 17, POS[i][1]);
  }
}

void zz(Canvas &c, int x, int y, int dir, uint32_t t) {   // two little z's floating up and away
  for (int k = 0; k < 2; k++) {
    uint32_t ph = (t + k * 1300) % 2600;
    int zy = y - (int)(ph * 5 / 2600), d = k * 3 + (int)(ph / 900);
    int zx = dir > 0 ? x + d : x - d - 2;
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

void catTail(const Pen &p, uint32_t t, bool swish = false) {
  bool flick = swish ? (t / 150) % 2 : (t % 3000) < 300;
  p.set(flick ? 9 : 8, 1); p.set(8, 2); p.set(8, 3); p.set(8, 4); p.set(7, 5);
}
void catEyes(const Pen &p, bool open, int look) {
  if (!open) row(p, 0, 3, "#######");
  else if (look > 0) row(p, 0, 3, "###.#.#");
  else if (look < 0) row(p, 0, 3, "#.#.###");
}

void moth(const Pen &p, int x, int y, uint32_t t) {
  if ((t / 120) % 2) { p.set(x - 1, y); p.set(x + 1, y); }
  else { p.set(x - 1, y + 1); p.set(x + 1, y + 1); }
  p.set(x, y + 1);
}

void catScene(const Pen &p, Period per, uint32_t t, int32_t a, uint8_t v) {
  if (per == Period::Night) {
    art(p, 0, 3, CAT_LOAF);
    if (a < 0) {
      if ((t % 3200) < 1600) p.hline(5, 8, 4);          // breathing: the back rises and falls
      zz(p.c, p.gx(p.flip ? 6 : 5), 2, p.flip ? -1 : 1, t);
    } else if (a < 1200) {
      if ((a / 200) % 2) p.set(2, 3, false);           // ear twitches
    } else if (a < 1600) {
      p.hline(0, 9, 2);                                 // big stretchy roll...
    }                                                    // ...and it ends up facing the other way
    return;
  }
  art(p, 0, 0, CAT);
  bool open = !blinking(t, L.salt);
  int look = glance(t, L.salt);
  uint32_t ua = a < 0 ? 0 : (uint32_t)a;
  switch (per) {
    case Period::Morning:
      if (a >= 0 && ua < 900) {                           // big yawn
        catEyes(p, false, 0); p.set(3, 4, false); p.set(3, 5, false);
        catTail(p, t);
      } else if (a >= 0 && ua >= 1300) {                 // then a happy tail swish
        catEyes(p, false, 0); catTail(p, t, true);
      } else { catEyes(p, open, look); catTail(p, t); }
      break;
    case Period::Day: {
      catTail(p, t);
      if (a < 0) { catEyes(p, open, look); break; }
      int startX = p.lx(p.flip ? -3 : MTX_W + 2);        // just off the far edge
      if ((v & 3) == 3) {
        // the moth flutters over and lands on the cat's head; the cat goes cross-eyed
        if (ua < 5000) {
          float q = ua / 5000.f;
          int mx = startX + (int)lroundf((3 - startX) * q);
          int my = (int)lroundf((1 - q) * (2 + sinf(ua * 0.004f) * 2) + q * -1);
          moth(p, mx, my, t);
          catEyes(p, true, q > 0.5f ? 1 : 0);
        } else if (ua < 6800) {
          moth(p, 3, -1, t);
          row(p, 0, 3, "###.###");
        } else {
          float q = (ua - 6800) / 1000.f;
          moth(p, 3 + (int)(q * 20), -1 + (int)lroundf(sinf(q * 7) * 1.5f), t);
          catEyes(p, true, 0);
        }
      } else {
        // the moth drifts in, the cat watches, swipes, the moth escapes
        catEyes(p, !(ua > 6350 && ua < 6500), ua > 4200 ? 1 : 0);
        if (ua < 6000) {
          float q = ua / 6000.f;
          int mx = startX + (int)lroundf((11 - startX) * q), my = 2 + (int)lroundf(sinf(ua * 0.004f) * 2);
          moth(p, mx, my, t);
        } else if (ua < 6350) {
          p.set(7, 4); p.set(8, 3); p.set(9, 3);           // paw swipe
          moth(p, 12 + (int)((ua - 6000) / 60), 1 - (int)((ua - 6000) / 120), t);
        } else if (ua < 7600) {
          float q = (ua - 6350) / 1250.f;
          moth(p, 18 + (int)(q * 16), -1 + (int)lroundf(sinf(q * 9) * 1.5f), t);
        }
      }
      break;
    }
    case Period::Evening: {
      bool groom = a >= 0 && (ua < 1400 || (ua >= 1800 && ua < 3200));   // two licking bouts
      catEyes(p, !groom && open, groom ? 0 : look);
      if (groom) {   // paw up to the face, licking
        int lift = ((ua / 200) % 2) ? 0 : 1;
        p.set(6, 4 - lift); p.set(7, 4 - lift); p.set(7, 5 - lift);
        p.set(7, 5); p.set(8, 6); p.set(8, 7);
      } else catTail(p, t + 1500);
      break;
    }
    default: break;
  }
}

// ---------------- frog ----------------
const char *const FROG_BODY[] = {
  ".#########.",
  "#.#.....#.#",
  "#..#####..#",   // smile
  ".#########.",
  "##.#...#.##",   // feet
};
const char *const NOTE[] = {".##", ".#.", "##.", "##."};

void frogEyes(const Pen &p, int y, bool open, int look) {
  if (!open) { row(p, 0, y + 1, ".##.....##."); row(p, 0, y + 2, "####...####"); return; }
  row(p, 0, y, ".##.....##.");
  row(p, 0, y + 1, "#..#...#..#");
  row(p, 0, y + 2, look > 0 ? "#.##...#.##" : look < 0 ? "##.#...##.#" : "#.##...##.#");
}
void frog(const Pen &p, int y, bool open, int look) {
  frogEyes(p, y, open, look);
  art(p, 0, y + 3, FROG_BODY);
}

// the fly loops around the open side of the screen
void flyAt(uint32_t t, int cx, int &x, int &y) {
  float s = t * 0.001f;
  x = cx + (int)lroundf(sinf(s * 1.3f) * 5 + sinf(s * 3.1f) * 2);
  y = 3 + (int)lroundf(sinf(s * 1.9f + 1) * 2 + sinf(s * 5.3f) * 0.8f);
}

void frogScene(const Pen &p, Period per, uint32_t t, int32_t a, uint8_t v) {
  bool open = !blinking(t, L.salt);
  int look = glance(t, L.salt);
  uint32_t ua = a < 0 ? 0 : (uint32_t)a;
  switch (per) {
    case Period::Morning:
      if (a >= 0 && ua < 900) {                          // big yawn
        frog(p, 0, false, 0);
        row(p, 0, 4, "#..#####..#"); row(p, 0, 5, "#..#...#..#");
      } else if (a >= 0 && ua >= 1300 && ua < 1700) {    // and a little stretch-hop
        frog(p, -1, true, 0);
      } else frog(p, 0, open, look);
      break;
    case Period::Day: {
      if (a < 0) { frog(p, 0, open, look); break; }
      bool miss = (v & 3) == 3;
      int edge = L.actOpen > MTX_W / 2 ? MTX_W + 3 : -4;
      int fx, fy;
      flyAt(t, L.actOpen, fx, fy);
      if (ua < 800) fx = edge + (int)lroundf((fx - edge) * ua / 800.f);   // buzzes in from the edge
      if (ua < 4000) {
        frog(p, 0, open, 1);
        if ((t / 40) % 3) p.c.set(fx, fy);              // buzzing flicker
      } else if (ua < 4400) {
        int hx, hy;
        flyAt(t - (ua - 4000), L.actOpen, hx, hy);      // where it was when the tongue left
        int lx = p.lx(hx), ly = hy + (miss ? (hy > 3 ? -3 : 3) : 0);
        float q = ua < 4200 ? (ua - 4000) / 200.f : 1 - (ua - 4200) / 200.f;
        int tx = 6 + (int)lroundf((lx - 6) * q), ty = 5 + (int)lroundf((ly - 5) * q);
        frog(p, 0, true, 1);
        row(p, 0, 5, "#..#...#..#");                     // mouth open
        line(p, 6, 5, tx, ty);
        if (miss) p.c.set(fx, fy);                       // it just keeps buzzing
        else if (ua < 4200) p.c.set(hx, hy); else p.set(tx, ty);
      } else if (miss) {
        float q = (ua - 4400) / 1000.f;                  // fly makes off; frog blinks, embarrassed
        p.c.set(fx + (int)lroundf((edge - fx) * q), fy - (int)(q * 3));
        frog(p, 0, !((ua > 4700 && ua < 4850) || (ua > 5000 && ua < 5150)), 1);
      } else {
        frog(p, 0, ua < 4700, 0);                        // gulp: eyes squeeze shut
        if (ua < 4900) row(p, 0, 5, "#.#######.#");
      }
      break;
    }
    case Period::Evening:
      frog(p, 0, open, a >= 0 ? 0 : look);
      if (a >= 0) {   // "ribbit": mouth pops open twice and a note floats up, 1-3 times
        uint32_t k = ua % 900;
        if (k < 800) {
          if ((k / 200) % 2 == 0) row(p, 0, 5, "#..#...#..#");
          float q = k / 800.f;
          art(p, 12 + (int)(q * 3), 3 - (int)lroundf(q * 4), NOTE);
        }
      }
      break;
    case Period::Night:
      frog(p, -1, false, 0);                            // asleep on a lily pad
      row(p, 0, 7, "#####.######");
      if (a >= 0 && ua >= 300 && ua < 2100) {            // one eye opens, peeks around, shuts
        row(p, 7, 0, "#..#");
        row(p, 7, 1, ua < 1100 ? "##.#" : ua < 1900 ? "#.##" : "#.##");
      } else if (a < 0) zz(p.c, p.gx(11), 1, p.flip ? -1 : 1, t);
      break;
  }
}

void drawPlain(const Pen &p) {   // mid-hop: the basic pose
  if (L.kind == Kind::Cat) { art(p, 0, 0, CAT); catTail(p, 0); }
  else frog(p, 0, true, 0);
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

void setPace(uint32_t div) { paceDiv = div ? div : 1; }

void begin(Kind k, uint32_t seed) {
  L = Life{};
  L.kind = k;
  L.rng = hash(seed) | 1;
  L.salt = rnd();
  L.x = randomSpot(-1);
  L.flip = onRight(L.x);
  L.skyRight = !L.flip;                 // sun/moon start on the open side
  L.peek = rnd() % 4 == 0;
  L.nextAct = paced(rnd(40000, 120000));
  L.nextMove = paced(rnd(60000, 150000));
}

void poke(uint32_t t) { if (!L.acting) L.nextAct = t; }

void draw(Canvas &c, Period p, uint32_t t) {
  update(p, t);

  // pose: where the creature is drawn this frame
  int w = width(), ox = L.x, oy = L.peek ? peekDepth() : 0;
  bool flip = L.flip, hopping = false, puff = false;
  if (L.mv == Mv::Hop) {
    uint32_t el = t - L.mvStart;
    int h = (int)(el / HOP_MS);
    if (h > L.hops - 1) h = L.hops - 1;
    float q = clamp01((el - h * HOP_MS) / (float)HOP_MS);
    ox = (int)lroundf(L.fromX + (L.toX - L.fromX) * (h + q) / L.hops);
    oy = -(int)lroundf(sinf(q * PI_F) * (abs(L.toX - L.fromX) > 6 ? 3 : 2));
    flip = L.toX < L.fromX;
    hopping = true;
    puff = h == L.hops - 1 && q > 0.85f;
  } else if (L.mv == Mv::Sink || L.mv == Mv::Rise) {
    float q = clamp01((t - L.mvStart) / (float)SINK_MS);
    oy = (int)lroundf((L.mv == Mv::Sink ? q : 1 - q) * peekDepth());
  }
  bool night = p == Period::Night;
  Period sp = L.acting ? L.actP : p;
  bool lying = !hopping && sp == Period::Night;

  // backdrop
  int skyX = L.skyRight ? 27 : 5;
  switch (p) {
    case Period::Morning: sun(c, skyX, t); break;
    case Period::Day: break;
    case Period::Evening:
      moon(c, skyX + 1, 2);
      stars(c, t, 3, 7, L.skyRight);
      if (L.kind == Kind::Frog) {   // fireflies glow on and off over the open side
        int cx = openCentre(ox);
        for (int i = 0; i < 3; i++) {
          float s = t * 0.0006f + i * 2.1f;
          int x = cx + (int)lroundf(sinf(s) * 6 + i - 1), y = 3 + (int)lroundf(sinf(s * 1.7f + i) * 2.5f);
          if ((t + i * 900) % 2700 < 1100) c.set(x, y);
        }
      }
      break;
    case Period::Night:
      moon(c, skyX + 2, 2);
      stars(c, t, L.kind == Kind::Cat ? 5 : 4, L.kind == Kind::Cat ? 3 : 11, L.skyRight);
      if (L.kind == Kind::Frog)   // slow ripples on the pond
        for (int x = 0; x < MTX_W; x++)
          if ((x + t / 400) % 7 == 0) c.set(x, 7);
      break;
  }
  (void)night;

  // the creature stands in front of the scenery: clear the space it occupies
  int top = lying ? (L.kind == Kind::Cat ? 3 : 0) : (oy > 0 ? oy : 0);
  for (int x = ox - 1; x <= ox + w; x++)
    for (int y = top; y < MTX_H; y++) c.set(x, y, false);

  Pen pen{c, ox, oy, w, flip};
  if (hopping) {
    drawPlain(pen);
    if (puff) { c.set(ox - 1, 7); c.set(ox + w, 7); }
    return;
  }
  if (lying) pen.oy = 0;
  int32_t a = L.acting ? (int32_t)(t - L.actStart) : -1;
  if (L.kind == Kind::Cat) catScene(pen, sp, t, a, L.variant);
  else frogScene(pen, sp, t, a, L.variant);
}

void drawHop(Canvas &c, float p) {
  int w = width();
  Pen pen{c, L.x, -(int)lroundf(sinf(p * PI_F) * 3), w, L.flip};
  drawPlain(pen);
  if (p > 0.85f) { c.set(L.x - 1, 7); c.set(L.x + w, 7); }   // landing puff
}
}  // namespace Critters
