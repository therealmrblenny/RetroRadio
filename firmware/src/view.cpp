#include "view.h"
#include <math.h>
#include "critters.h"

namespace View {
namespace {

// ======================================================================
// helpers
// ======================================================================
float clamp01(float p) { return p < 0 ? 0 : p > 1 ? 1 : p; }
float prog(uint32_t now, uint32_t start, uint32_t dur) { return dur ? clamp01((float)(int32_t)(now - start) / dur) : 1; }
float easeOutCubic(float p) { p = 1 - p; return 1 - p * p * p; }
float easeInOut(float p) { return p < 0.5f ? 4 * p * p * p : 1 - powf(-2 * p + 2, 3) / 2; }
float easeOutBack(float p) { const float c = 1.9f; p -= 1; return 1 + (c + 1) * p * p * p + c * p * p; }
float easeOutBounce(float p) {
  const float n = 7.5625f, d = 2.75f;
  if (p < 1 / d) return n * p * p;
  if (p < 2 / d) { p -= 1.5f / d; return n * p * p + 0.75f; }
  if (p < 2.5f / d) { p -= 2.25f / d; return n * p * p + 0.9375f; }
  p -= 2.625f / d; return n * p * p + 0.984375f;
}
uint32_t hash(uint32_t x) {   // deterministic noise (same on PC and board)
  x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16; return x;
}
uint32_t frameNo(uint32_t now) { return now / FRAME_MS; }
void noise(Canvas &c, uint32_t seed, int density /*0..255*/) {
  for (int x = 0; x < MTX_W; x++)
    for (int y = 0; y < MTX_H; y++)
      if ((hash(seed * 977 + x * 31 + y) & 255) < (uint32_t)density) c.set(x, y);
}

Font fnt = Font::Compact;
int userBright = 3;

// ---- icons (column bits, bit0 = top) ----
// wifi: 7 wide, three layers so it can pulse: dot, small arc, big arc
const uint8_t WIFI_L0[7] = {0, 0, 0, 0x40, 0, 0, 0};
const uint8_t WIFI_L1[7] = {0, 0x10, 0x08, 0x08, 0x08, 0x10, 0};
const uint8_t WIFI_L2[7] = {0x04, 0x02, 0x02, 0x02, 0x02, 0x02, 0x04};
const uint8_t FOLDER[6] = {0x7E, 0x46, 0x46, 0x44, 0x44, 0x7C};
const uint8_t BACK[4] = {0x08, 0x1C, 0x3E, 0x08};   // small left arrow
const uint8_t SPEAKER[4] = {0x1C, 0x1C, 0x3E, 0x7F};

int iconWidth(Icon i) { return i == Icon::Wifi ? 7 : i == Icon::Folder ? 6 : i == Icon::Back ? 4 : 0; }
void drawIcon(Canvas &c, Icon i, int x, int y = 0, int layers = 3) {
  for (int k = 0; k < iconWidth(i); k++) {
    uint8_t b = 0;
    if (i == Icon::Wifi) b = (layers > 0 ? WIFI_L0[k] : 0) | (layers > 1 ? WIFI_L1[k] : 0) | (layers > 2 ? WIFI_L2[k] : 0);
    else if (i == Icon::Folder) b = FOLDER[k];
    else if (i == Icon::Back) b = BACK[k];
    c.column(x + k, b, y);
  }
}

// ======================================================================
// Marquee: text in a horizontal region [x0, x0+w).
//   Loop:  fits -> centred and still; too long -> hold, scroll 1 px/frame, loop with a gap.
//   Once:  fits -> held; too long -> hold, scroll to the end, hold. durationMs() says how long.
//   loops: Loop mode only, 0 = forever, else park at the start after that many passes.
//   still: never moves (fits -> centred, too long -> shows the start).
// ======================================================================
struct Marquee {
  std::string text;
  uint32_t start = 0;
  bool loop = true;
  uint32_t holdMs = 1500;
  int loops = 0;
  bool still = false;
  static constexpr int GAP = 10;

  bool set(const std::string &t, uint32_t now) {
    if (t == text) return false;
    text = t; start = now; return true;
  }
  int width() const { return Canvas::textWidth(text.c_str(), fnt); }
  uint32_t durationMs(int w = MTX_W) const {
    int tw = width();
    if (tw <= w) return 2200;
    return holdMs + (tw - w) * FRAME_MS + 900;
  }
  // a Loop marquee with `loops` set has made its passes and parked at the start
  bool done(uint32_t now, int w = MTX_W) const {
    int tw = width();
    if (!loop || loops <= 0 || tw <= w) return false;
    return now - start >= (uint32_t)loops * (holdMs + (tw + GAP) * FRAME_MS);
  }
  // x offset of the text for this frame (relative to the region)
  int offset(uint32_t now, int w, bool centre, int &copyAt) const {
    int tw = width();
    copyAt = 0x7FFF;
    if (tw <= w) return centre ? (w - tw) / 2 : 0;
    if (still || done(now, w)) return 0;
    uint32_t t = now - start;
    if (!loop) {
      if (t < holdMs) return 0;
      int px = (t - holdMs) / FRAME_MS;
      return -(px < tw - w ? px : tw - w);
    }
    uint32_t cycle = holdMs + (tw + GAP) * FRAME_MS;
    t %= cycle;
    if (t < holdMs) return 0;
    int off = -(int)((t - holdMs) / FRAME_MS);
    copyAt = off + tw + GAP;
    return off;
  }
  bool moving(uint32_t now, int w) const {
    int tw = width();
    if (tw <= w || still || done(now, w)) return false;
    uint32_t t = now - start;
    if (!loop) return t >= holdMs && (int)((t - holdMs) / FRAME_MS) < tw - w;
    return t % (holdMs + (tw + GAP) * FRAME_MS) >= holdMs;
  }
  void draw(Canvas &c, uint32_t now, int x0 = 0, int w = MTX_W, int y = 0, bool centre = true) const {
    Canvas tmp; tmp.clear();
    int copyAt, off = offset(now, w, centre, copyAt);
    // Standing still: nudge by a pixel so a blank column sits on the hidden gap
    // between the modules instead of a letter stroke.
    int gap = MTX_GAP_COL - x0;
    if (MTX_GAP_COL >= 0 && gap >= 0 && gap < w && !moving(now, w)) {
      static const int8_t TRY[] = {0, 1, -1, 2, -2};
      int tw = width();
      for (int d : TRY) {
        if (off >= 0 && off + d < 0) continue;              // never clip the first letter
        if (tw <= w && off + d + tw > w) continue;          // or the last
        Canvas probe; probe.clear();
        probe.text(off + d, text.c_str(), y, fnt);
        if (!probe.col[gap]) { off += d; break; }
      }
    }
    tmp.text(off, text.c_str(), y, fnt);
    if (copyAt < w) tmp.text(copyAt, text.c_str(), y, fnt);
    for (int i = 0; i < w && x0 + i < MTX_W; i++) c.column(x0 + i, tmp.col[i]);
  }
};

// ======================================================================
// Transitions: blend the previous frame into the new scene
// ======================================================================
enum class Tr : uint8_t { None, SlideUp, SlideDown, WipeRight, Dissolve };
struct Transition {
  Tr type = Tr::None; uint32_t start = 0, dur = 0; Canvas from{};
  bool active(uint32_t now) const { return type != Tr::None && now - start < dur; }
} trans;
Canvas last{};   // previous output frame

void beginTransition(uint32_t now, Tr t, uint32_t dur) {
  trans.type = t; trans.start = now; trans.dur = dur; trans.from = last;
}

void composite(uint32_t now, const Canvas &to, Canvas &out) {
  float p = prog(now, trans.start, trans.dur);
  const Canvas &fr = trans.from;
  switch (trans.type) {
    case Tr::SlideUp: case Tr::SlideDown: {
      int k = (int)lroundf(easeOutCubic(p) * MTX_H);
      for (int x = 0; x < MTX_W; x++) {
        uint16_t a = fr.col[x], b = to.col[x];
        out.col[x] = trans.type == Tr::SlideUp ? (uint8_t)((a >> k) | (b << (MTX_H - k)))
                                               : (uint8_t)((a << k) | (b >> (MTX_H - k)));
      }
      break;
    }
    case Tr::WipeRight: {
      int e = (int)(easeInOut(p) * (MTX_W + 2));
      for (int x = 0; x < MTX_W; x++) out.col[x] = x < e - 1 ? to.col[x] : x <= e ? 0xFF : fr.col[x];
      break;
    }
    case Tr::Dissolve:
      out.clear();
      for (int x = 0; x < MTX_W; x++)
        for (int y = 0; y < MTX_H; y++) {
          bool useNew = (hash(x * 8 + y + 1234) & 255) < (uint32_t)(p * 256);
          if (useNew ? to.get(x, y) : fr.get(x, y)) out.set(x, y);
        }
      break;
    default: out = to; break;
  }
}

// ======================================================================
// scene state
// ======================================================================
enum class Sc : uint8_t { Boot, Connecting, Pages, Tuning, Playing, Browse, Standby, Waking };
Sc sc = Sc::Boot;
uint32_t scStart = 0;

void go(uint32_t now, Sc s, Tr t = Tr::None, uint32_t dur = 0) {
  sc = s; scStart = now;
  if (t != Tr::None) beginTransition(now, t, dur);
}

// now playing
std::string npName, npTitle;
bool npBuffering = false, npStalled = false;
Marquee playMq;
Marquee nameMq;                // the station name alone, once the marquee settles
bool npSettled = false;
uint32_t nameNext = 0;         // when a too-long name next scrolls past once
const int PLAY_SCROLLS = 2;    // passes of "name > title" before settling on the name
const uint32_t NAME_REPEAT_MS = 60000;

std::string playText() {
  std::string s = npName;
  if (npStalled) s += " \x7f NO SIGNAL";
  else if (!npTitle.empty()) s += " \x7f " + npTitle;
  return s;
}

// tuning
struct { std::string name; int pos = 0, count = 1; float needleFrom = 2; bool pending = false; } tune;
float needleAt = 2;     // where the needle was left last time
const uint32_t TUNE_STATIC = 220, TUNE_SWEEP = 520, TUNE_DROP = 420;
const uint32_t TUNE_MS = TUNE_STATIC + TUNE_SWEEP + TUNE_DROP;

// pages
std::vector<Page> pages;
bool pagesLoop = false, pagesFinished = false;
size_t pageIdx = 0;
uint32_t pageStart = 0;
Marquee pageMq;

// browse
Marquee browseMq;
Icon browseIcon = Icon::None;
int browsePos = 0, browseCount = 1;

// volume overlay
const uint32_t VOL_SHOW_MS = 1500;
uint32_t volUntil = 0;
int volTarget = 0, volMax = 21;
float volFill = 0, peakPos = 0, peakVel = 0;
uint32_t volLastT = 0;
bool volWasShown = false;

// standby / wake
const uint32_t SLEEP_FADE_START = 500, SLEEP_FADE_END = 1500, WAKE_MS = 900;
Critters::Kind critter = Critters::Kind::Cat;
int clockMin = -1;   // minutes since local midnight, -1 = not known yet

// ======================================================================
// scene drawing
// ======================================================================
void drawBoot(uint32_t now, Canvas &c) {
  uint32_t t = now - scStart;
  if (t < 650) {                         // light sweep with a sparkly trail
    int head = -3 + (int)(easeInOut(t / 650.f) * (MTX_W + 10));
    for (int k = 0; k < 10; k++) {
      int x = head - k;
      if (k < 2) { c.column(x, 0xFF); continue; }
      for (int y = 0; y < MTX_H; y++)
        if ((int)(hash(frameNo(now) * 131 + x * 8 + y) % 10) >= k) c.set(x, y);
    }
    return;
  }
  const char *word = t < 2000 ? "RETRO" : "RADIO";
  Font f = Font::Compact;              // logo always in compact caps (fits either word)
  int w = Canvas::textWidth(word, f), x0 = (MTX_W - w) / 2;
  if (t < 2000) {                        // letters slam down one by one
    char s[2] = {0, 0};
    int x = x0;
    for (int i = 0; word[i]; i++) {
      s[0] = word[i];
      int lw = Canvas::textWidth(s, f);
      int32_t lt = (int32_t)t - 700 - i * 140;
      if (lt >= 0) {
        int y = -8 + (int)lroundf(easeOutBounce(clamp01(lt / 300.f)) * 8);
        c.text(x, s, y, f);
        if (lt > 120 && lt < 200) c.hline(x - 1, x + lw, 7);   // impact flash
      }
      x += lw + 1;
    }
    return;
  }
  if (t < 2300) {                        // RETRO rolls up, RADIO comes in from below
    Canvas a; a.clear(); a.text((MTX_W - Canvas::textWidth("RETRO", f)) / 2, "RETRO", 0, f);
    Canvas b; b.clear(); b.text(x0, word, 0, f);
    int k = (int)lroundf(easeOutCubic((t - 2000) / 300.f) * MTX_H);
    for (int x = 0; x < MTX_W; x++) c.col[x] = (uint8_t)((a.col[x] >> k) | ((uint16_t)b.col[x] << (MTX_H - k)));
    return;
  }
  c.text(x0, word, 0, f);
  if (t > 2450 && t < 2850) {            // diagonal glint
    int g = (int)((t - 2450) / 400.f * (MTX_W + 12)) - 6;
    for (int x = 0; x < MTX_W; x++)
      for (int y = 0; y < 7; y++) {
        int d = x + y / 2 - g;
        if (d >= 0 && d < 2) c.set(x, y, !c.get(x, y));
      }
  }
}
const uint32_t BOOT_MS = 3100;

void drawConnecting(uint32_t now, Canvas &c) {
  uint32_t ph = ((now - scStart) / 300) % 5;   // 0..2 grow, 3 full, 4 full
  drawIcon(c, Icon::Wifi, 1, 0, ph >= 3 ? 3 : (int)ph + 1);
  const char *w = "WIFI";
  int tw = Canvas::textWidth(w, fnt), rx = 10, rw = MTX_W - rx;
  c.text(rx + (rw - tw) / 2, w, 0, fnt);
  // dots marching under the text
  int d = ((now - scStart) / 200) % 4;
  for (int i = 0; i < 3; i++) if (i < d) c.set(rx + rw / 2 - 3 + i * 3, 7);
}

uint32_t pageDuration(const Page &p) {
  int w = MTX_W - (p.icon != Icon::None ? iconWidth(p.icon) + 2 : 0);
  return pageMq.durationMs(w);
}
void startPage(uint32_t now, size_t i) {
  pageIdx = i; pageStart = now;
  pageMq.loop = false; pageMq.holdMs = 900;
  pageMq.text.clear(); pageMq.set(pages[i].text, now);
}
void drawPages(uint32_t now, Canvas &c) {
  if (pages.empty()) return;
  if (!pagesFinished && now - pageStart >= pageDuration(pages[pageIdx])) {
    if (pageIdx + 1 < pages.size()) { startPage(now, pageIdx + 1); beginTransition(now, Tr::SlideUp, 260); }
    else if (pagesLoop) { startPage(now, 0); beginTransition(now, Tr::SlideUp, 260); }
    else pagesFinished = true;
  }
  const Page &p = pages[pageIdx];
  int x0 = 0;
  if (p.icon != Icon::None) {
    int layers = p.icon == Icon::Wifi ? (int)((now / 350) % 4) : 3;
    drawIcon(c, p.icon, 0, 0, layers == 0 ? 3 : layers);
    x0 = iconWidth(p.icon) + 2;
  }
  pageMq.draw(c, now, x0, MTX_W - x0);
}

int needleX(int pos, int count) { return count > 1 ? 2 + pos * (MTX_W - 5) / (count - 1) : MTX_W / 2; }

void drawPlayingText(uint32_t now, Canvas &c, int y = 0);
void drawTuning(uint32_t now, Canvas &c) {
  uint32_t t = now - scStart;
  if (t < TUNE_STATIC) {                       // burst of static, thinning out
    noise(c, frameNo(now), 120 - (int)(t * 60 / TUNE_STATIC));
    return;
  }
  float target = (float)needleX(tune.pos, tune.count);
  if (t < TUNE_STATIC + TUNE_SWEEP) {
    for (int x = 0; x < MTX_W; x += 2) c.set(x, 7);   // dial scale on the bottom row
    float p = easeOutBack((t - TUNE_STATIC) / (float)TUNE_SWEEP);
    float nx = tune.needleFrom + (target - tune.needleFrom) * p;
    int x = (int)lroundf(nx);
    c.vline(x, 0, 7);
    if ((frameNo(now) & 1) == 0) noise(c, frameNo(now) + 7, 18);   // faint hiss while tuning
    return;
  }
  // station name drops in and bounces into place; the dial shrinks into the needle
  float p = clamp01((t - TUNE_STATIC - TUNE_SWEEP) / (float)TUNE_DROP);
  int y = -8 + (int)lroundf(easeOutBounce(p) * 8);
  playMq.set(playText(), now);
  playMq.start = now;                          // keep the marquee parked at its first frame
  drawPlayingText(now, c, y);
  int reach = (int)((1 - easeOutCubic(p)) * MTX_W);
  for (int x = 0; x < MTX_W; x += 2) if (fabsf(x - target) <= reach) c.set(x, 7);
}

void drawPlayingText(uint32_t now, Canvas &c, int y) {
  Canvas tmp; tmp.clear();
  playMq.draw(tmp, now);
  for (int x = 0; x < MTX_W; x++) c.column(x, tmp.col[x] & 0x7F, y);   // row 7 is the status row
}

void drawPlaying(uint32_t now, Canvas &c) {
  playMq.loops = npStalled ? 0 : PLAY_SCROLLS;   // keep NO SIGNAL going
  if (playMq.set(playText(), now)) {            // new song: scroll again
    if (now - scStart > 100) beginTransition(now, Tr::SlideUp, 300);
    npSettled = false;
  }
  if (!npSettled && playMq.done(now)) {         // parked on the name: drop the title
    npSettled = true;
    beginTransition(now, Tr::Dissolve, 500);
    nameMq.loops = 1; nameMq.still = true; nameNext = now + NAME_REPEAT_MS;
  }
  if (npSettled) {
    nameMq.set(npName, now);
    if ((int32_t)(now - nameNext) >= 0) {      // long name: one slow pass, then rest again
      nameMq.still = false; nameMq.start = now; nameNext = now + NAME_REPEAT_MS;
    }
    Canvas tmp; tmp.clear();
    nameMq.draw(tmp, now);
    for (int x = 0; x < MTX_W; x++) c.column(x, tmp.col[x] & 0x7F);
  } else drawPlayingText(now, c);
  if (npStalled && hash(now / 3000) % 3 == 0 && now % 3000 < 160) noise(c, frameNo(now), 70);
  if (npBuffering) {                          // runner on the status row
    int x = (now / 60) % (MTX_W * 2);
    if (x >= MTX_W) x = MTX_W * 2 - 1 - x;
    c.set(x, 7); c.set(x - 1, 7);
  }
}

void drawBrowse(uint32_t now, Canvas &c) {
  int x0 = browseIcon != Icon::None ? iconWidth(browseIcon) + 2 : 0;
  if (x0) drawIcon(c, browseIcon, 0);
  Canvas tmp; tmp.clear();
  browseMq.draw(tmp, now, x0, MTX_W - x0, 0, x0 == 0);
  for (int x = 0; x < MTX_W; x++) c.column(x, tmp.col[x] & 0x7F);
  // position on the list: a short bar on the bottom row
  int n = browseCount < 1 ? 1 : browseCount;
  int len = MTX_W / n; if (len < 2) len = 2; if (len > 8) len = 8;
  int x = n > 1 ? browsePos * (MTX_W - len) / (n - 1) : (MTX_W - len) / 2;
  c.hline(x, x + len - 1, 7);
}

void drawVolume(uint32_t now, Canvas &c) {
  // fill eases to the target; the peak marker overshoots on a spring and settles
  float dt = clamp01((now - volLastT) / 1000.f);
  volLastT = now;
  const int x0 = 6, w = MTX_W - x0;
  float target = (float)volTarget * w / volMax;
  volFill += (target - volFill) * clamp01(dt * 18);
  float acc = (target - peakPos) * 260 - peakVel * 14;
  peakVel += acc * dt; peakPos += peakVel * dt;
  for (int i = 0; i < 4; i++) c.column(i, SPEAKER[i]);
  int fill = (int)lroundf(volFill);
  for (int i = 0; i < w; i++) {
    if (i < fill) c.column(x0 + i, 0x3C);
    else if (i % 2 == 0) c.set(x0 + i, 4);
  }
  int pk = (int)lroundf(peakPos);
  if (pk < 0) pk = 0;
  if (pk > w - 1) pk = w - 1;
  if (volTarget > 0) c.column(x0 + pk, 0x7E);
}

void drawStandby(uint32_t now, Canvas &c) {
  Critters::draw(c, Critters::periodFor(clockMin), now - scStart);
}

void drawWaking(uint32_t now, Canvas &c) {
  float p = clamp01((now - scStart) / 550.f);
  Critters::drawHop(c, p);
}

void startTuning(uint32_t now, Tr t, uint32_t dur) {
  tune.needleFrom = needleAt;
  needleAt = (float)needleX(tune.pos, tune.count);
  tune.pending = false;
  npSettled = false;
  playMq.text.clear();
  playMq.set(playText(), now);
  go(now, Sc::Tuning, t, dur);
}

}  // namespace

// ======================================================================
// public API
// ======================================================================
void setFont(Font f) { fnt = f; playMq.text.clear(); nameMq.text.clear(); browseMq.text.clear(); pageMq.text.clear(); }
Font font() { return fnt; }
void setUserBrightness(int b) { userBright = b; }

void showBoot(uint32_t now) { go(now, Sc::Boot); }
bool bootDone(uint32_t now) { return sc != Sc::Boot || now - scStart >= BOOT_MS; }

void showConnecting(uint32_t now) { go(now, Sc::Connecting, Tr::Dissolve, 350); }

void showPages(uint32_t now, const std::vector<Page> &p, bool loop) {
  pages = p; pagesLoop = loop; pagesFinished = false;
  if (pages.empty()) pages.push_back({""});
  startPage(now, 0);
  go(now, Sc::Pages, Tr::SlideUp, 300);
}
bool pagesDone(uint32_t now) {
  if (sc != Sc::Pages) return true;
  return pagesFinished || (!pagesLoop && pageIdx + 1 == pages.size() && now - pageStart >= pageDuration(pages[pageIdx]));
}

void tuneTo(uint32_t now, const char *name, int pos, int count) {
  tune.name = name; tune.pos = pos; tune.count = count < 1 ? 1 : count;
  npName = name; npTitle.clear(); npStalled = false;
  if (sc == Sc::Waking) { tune.pending = true; return; }   // after the wake-up blink
  startTuning(now, sc == Sc::Playing || sc == Sc::Browse ? Tr::None : Tr::Dissolve, 250);
}

void showPlaying(uint32_t now) {
  playMq.set(playText(), now);
  go(now, Sc::Playing, Tr::SlideDown, 220);
}

void setNowPlaying(const char *name, const char *title, bool buffering, bool stalled) {
  npName = name; npTitle = title; npBuffering = buffering; npStalled = stalled;
}

void showVolume(uint32_t now, int vol, int max) {
  volMax = max < 1 ? 1 : max;
  if (now >= volUntil) {   // fresh overlay: start the bar where the volume was
    volFill = peakPos = (float)volTarget * (MTX_W - 6) / volMax;
    peakVel = 0; volLastT = now;
  }
  volTarget = vol < 0 ? 0 : vol > volMax ? volMax : vol;
  volUntil = now + VOL_SHOW_MS;
}
void hideVolume() { volUntil = 0; }

void showBrowse(uint32_t now, const char *label, int pos, int count, int dir, Icon icon) {
  browseMq.loop = true; browseMq.holdMs = 700;
  browseMq.text.clear(); browseMq.set(label, now);
  browseIcon = icon; browsePos = pos; browseCount = count;
  volUntil = 0;
  if (dir > 0) go(now, Sc::Browse, Tr::SlideUp, 130);
  else if (dir < 0) go(now, Sc::Browse, Tr::SlideDown, 130);
  else go(now, Sc::Browse, Tr::SlideUp, 220);
}

void setClock(int minuteOfDay) { clockMin = minuteOfDay; }

void showStandby(uint32_t now, uint32_t seed) {
  critter = (seed & 1) ? Critters::Kind::Frog : Critters::Kind::Cat;
  Critters::begin(critter, seed >> 1);
  volUntil = 0;
  go(now, Sc::Standby, Tr::Dissolve, 400);
}

void wake(uint32_t now) { go(now, Sc::Waking); }
void pokeCritter(uint32_t now) { if (sc == Sc::Standby) Critters::poke(now - scStart); }

bool busy(uint32_t now) {
  return (sc == Sc::Boot && now - scStart < BOOT_MS) || sc == Sc::Waking ||
         (sc == Sc::Tuning && now - scStart < TUNE_MS);
}

int preview(const char *text, Canvas &out) {
  Marquee m; m.still = true; m.text = text;
  Canvas tmp; tmp.clear(); m.draw(tmp, 0);
  out.clear();
  for (int x = 0; x < MTX_W; x++) out.column(x, tmp.col[x] & 0x7F);
  return m.width();
}

int render(uint32_t now, Canvas &out) {
  // scene hand-offs that happen by themselves
  if (sc == Sc::Waking && now - scStart >= WAKE_MS) {
    if (tune.pending) startTuning(now, Tr::WipeRight, 280);
    else go(now, Sc::Playing, Tr::WipeRight, 280);
  }
  if (sc == Sc::Tuning && now - scStart >= TUNE_MS) { sc = Sc::Playing; scStart = now; playMq.start = now; }

  Canvas cur; cur.clear();
  bool volShown = now < volUntil && sc != Sc::Standby && sc != Sc::Waking;
  if (volShown != volWasShown) beginTransition(now, Tr::Dissolve, volShown ? 120 : 260);
  volWasShown = volShown;

  if (volShown) drawVolume(now, cur);
  else switch (sc) {
    case Sc::Boot: drawBoot(now, cur); break;
    case Sc::Connecting: drawConnecting(now, cur); break;
    case Sc::Pages: drawPages(now, cur); break;
    case Sc::Tuning: drawTuning(now, cur); break;
    case Sc::Playing: drawPlaying(now, cur); break;
    case Sc::Browse: drawBrowse(now, cur); break;
    case Sc::Standby: drawStandby(now, cur); break;
    case Sc::Waking: drawWaking(now, cur); break;
  }

  if (trans.active(now)) composite(now, cur, out);
  else { out = cur; trans.type = Tr::None; }
  last = out;

  if (sc == Sc::Standby) {
    uint32_t t = now - scStart;
    if (t < SLEEP_FADE_START) return -1;
    if (t < SLEEP_FADE_END) return (int)lroundf(userBright * (1 - (t - SLEEP_FADE_START) / (float)(SLEEP_FADE_END - SLEEP_FADE_START)));
    return 0;
  }
  return -1;
}

}  // namespace View
