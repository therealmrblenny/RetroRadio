#include "ui.h"
#include "canvas.h"
#include "matrix.h"
#include "input.h"
#include "radio.h"
#include "stations.h"
#include "net.h"
#include <Preferences.h>

namespace {
UI::Mode md = UI::Mode::Boot;
Canvas cv;
uint8_t shown[MTX_W];
portMUX_TYPE fbMux = portMUX_INITIALIZER_UNLOCKED;

// persisted settings
int vol = 8, idx = 0, bright = 3;
bool standby = false;
int testPat = 0;  // 0 = off; see drawTest()
bool settingsDirty = false;
uint32_t settingsChanged = 0;

uint32_t modeSince = 0, volOverlayUntil = 0, lastInput = 0;
int browseSel = 0;

// ---------- marquee scroller ----------
struct Scroller {
  String text; uint32_t start = 0;
  void set(const String &t) { if (t != text) { text = t; start = millis(); } }
  // Fits: centred and still. Too long: hold 1.2s, then scroll seamlessly at ~22 px/s
  void draw(Canvas &c, int y = 0) {
    int w = Canvas::textWidth(text.c_str());
    if (w <= MTX_W) { c.text((MTX_W - w) / 2, text.c_str(), y); return; }
    const int gap = 12;
    uint32_t t = millis() - start;
    int off = t < 1200 ? 0 : (int)((t - 1200) * 22 / 1000) % (w + gap);
    c.text(-off, text.c_str(), y);
    c.text(-off + w + gap, text.c_str(), y);
  }
} scroll;

void markSettings() { settingsDirty = true; settingsChanged = millis(); }

void loadSettings() {
  Preferences p; p.begin("radio", true);
  vol = p.getInt("vol", 8); idx = p.getInt("idx", 0);
  bright = p.getInt("bright", 3); standby = p.getBool("standby", false);
  p.end();
}
void saveSettings() {
  Preferences p; p.begin("radio", false);
  p.putInt("vol", vol); p.putInt("idx", idx); p.putInt("bright", bright); p.putBool("standby", standby);
  p.end();
  settingsDirty = false;
}

void setMode(UI::Mode m) { md = m; modeSince = millis(); }

void tune(int i) {
  size_t n = Stations::count();
  if (!n) return;
  idx = ((i % (int)n) + (int)n) % (int)n;
  Station s;
  if (Stations::get(idx, s)) Radio::play(s.name.c_str(), s.url.c_str());
  markSettings();
}

void enterStandby() {
  standby = true; markSettings();
  Radio::stop();
  setMode(UI::Mode::Standby);
}
void wake() {
  standby = false; markSettings();
  setMode(UI::Mode::Playing);
  tune(idx);
}
void changeVolume(int v) {
  vol = constrain(v, 0, VOL_MAX);
  Radio::setVolume(vol);
  volOverlayUntil = millis() + VOL_OVERLAY_MS;
  markSettings();
}

// ---------- input ----------
void handle(const Action &a) {
  lastInput = millis();
  Serial.printf("[ui] action %d v=%ld in mode %d\n", (int)a.type, (long)a.v, (int)md);
  using A = Act;
  // direct commands work in any mode
  switch (a.type) {
    case A::SetVolume: if (md != UI::Mode::Standby) changeVolume(a.v); return;
    case A::Play: standby = false; setMode(UI::Mode::Playing); tune(a.v); return;
    case A::Next: if (md == UI::Mode::Playing) tune(idx + 1); return;
    case A::Prev: if (md == UI::Mode::Playing) tune(idx - 1); return;
    case A::Standby: if (a.v && md != UI::Mode::Standby) enterStandby(); else if (!a.v && md == UI::Mode::Standby) wake(); return;
    case A::SetBrightness: bright = constrain(a.v, 0, 7); markSettings(); return;
    case A::StationsChanged:
      if (idx >= (int)Stations::count()) idx = 0;
      return;
    default: break;
  }
  // knob
  switch (md) {
    case UI::Mode::Playing:
      if (a.type == A::TurnCW) changeVolume(vol + 1);
      else if (a.type == A::TurnCCW) changeVolume(vol - 1);
      else if (a.type == A::Press) { browseSel = idx; volOverlayUntil = 0; setMode(UI::Mode::Browse); }
      else if (a.type == A::LongPress) enterStandby();
      break;
    case UI::Mode::Browse: {
      int n = Stations::count();
      if (a.type == A::TurnCW) browseSel = (browseSel + 1) % n;
      else if (a.type == A::TurnCCW) browseSel = (browseSel - 1 + n) % n;
      else if (a.type == A::Press) { setMode(UI::Mode::Playing); if (browseSel != idx || !Radio::snapshot().wantPlaying) tune(browseSel); }
      else if (a.type == A::LongPress) setMode(UI::Mode::Playing);
      break;
    }
    case UI::Mode::Standby:
      if (a.type == A::Press || a.type == A::LongPress) wake();
      break;
    default: break;
  }
}

// ---------- drawing ----------
void drawRunnerDot(uint32_t t) {   // bottom-row activity dot (buffering / connecting)
  int x = (t / 60) % (MTX_W * 2);
  if (x >= MTX_W) x = MTX_W * 2 - 1 - x;
  cv.set(x, 7); cv.set(x - 1, 7);
}

void drawVolume() {
  // speaker icon + bar
  const uint8_t spk[] = {0x1C, 0x1C, 0x3E, 0x7F};
  for (int i = 0; i < 4; i++) cv.column(i, spk[i]);
  int x0 = 6, w = MTX_W - x0;
  int fill = (vol * w + VOL_MAX / 2) / VOL_MAX;
  for (int i = 0; i < w; i++) {
    if (i < fill) cv.column(x0 + i, 0x3C);       // rows 2-5 lit
    else if (i % 2 == 0) cv.set(x0 + i, 4);      // dotted track
  }
}

void drawSleepy(uint32_t t) {
  // face drifts slowly so it isn't static; "z"s float up from it
  int span = MTX_W - 19;  // face + zZ is 19px wide
  int step = (t / 4000) % (span * 2);
  int fx = step < span ? step : span * 2 - step;
  // closed eyes (little arcs) and a small mouth
  const int8_t eye[][2] = {{0, 2}, {1, 3}, {2, 3}, {3, 2}};
  for (auto &p : eye) { cv.set(fx + p[0], p[1] + 2); cv.set(fx + 6 + p[0], p[1] + 2); }
  cv.set(fx + 4, 7); cv.set(fx + 5, 7);
  // z (3x3) rising over 2.4s, second one offset
  for (int k = 0; k < 2; k++) {
    uint32_t ph = (t + k * 1200) % 2400;
    int zy = 3 - (int)(ph * 5 / 2400);
    int zx = fx + 12 + k * 4;  // clear of the eyes and of each other
    if (zy < 0) continue;
    cv.set(zx, zy); cv.set(zx + 1, zy); cv.set(zx + 2, zy);
    cv.set(zx + 1, zy + 1);
    cv.set(zx, zy + 2); cv.set(zx + 1, zy + 2); cv.set(zx + 2, zy + 2);
  }
}

// Wiring test patterns (web: /api/cmd?c=test&v=N, 0 = off)
//  1 orientation: solid 3x3 block top-left, "L" left half, "R" right half
//  2 column walker: one lit column stepping left->right
//  3 row walker: one lit row stepping top->bottom
//  4 all pixels on
void drawTest(uint32_t now) {
  switch (testPat) {
    case 1:
      for (int x = 0; x < 3; x++) for (int y = 0; y < 3; y++) cv.set(x, y);
      cv.text(6, "L");
      cv.text(22, "R");
      break;
    case 2: cv.column((now / 250) % MTX_W, 0xFF); break;
    case 3: for (int x = 0; x < MTX_W; x++) cv.set(x, (now / 400) % MTX_H); break;
    case 4: for (int x = 0; x < MTX_W; x++) cv.column(x, 0xFF); break;
  }
}

void render() {
  uint32_t now = millis();
  cv.clear();
  if (testPat) {
    drawTest(now);
    Matrix::setBrightness(bright);
    portENTER_CRITICAL(&fbMux);
    memcpy(shown, cv.col, MTX_W);
    portEXIT_CRITICAL(&fbMux);
    Matrix::show(cv.col);
    return;
  }
  switch (md) {
    case UI::Mode::Boot:
      scroll.set("HELLO"); scroll.draw(cv);
      break;
    case UI::Mode::Connecting:
      scroll.set("WIFI"); scroll.draw(cv); drawRunnerDot(now);
      break;
    case UI::Mode::SetupAP:
      scroll.set(String("SETUP: join wifi \"") + AP_SSID + "\" then open 192.168.4.1");
      scroll.draw(cv);
      break;
    case UI::Mode::Playing: {
      if (now < volOverlayUntil) { drawVolume(); break; }
      auto s = Radio::snapshot();
      bool stalled = s.wantPlaying && !s.running && now - s.sinceMs > 12000;
      if (stalled) scroll.set(String(s.name) + " \x7f NO SIGNAL");
      else if (s.title[0]) scroll.set(String(s.name) + " \x7f " + s.title);
      else scroll.set(s.name);
      scroll.draw(cv);
      if (s.wantPlaying && !s.running) drawRunnerDot(now);
      break;
    }
    case UI::Mode::Browse: {
      Station st; Stations::get(browseSel, st);
      scroll.set(String(browseSel + 1) + " " + st.name);
      scroll.draw(cv);
      // bottom row: position indicator across the list
      int n = max<int>(1, Stations::count());
      int x = n > 1 ? browseSel * (MTX_W - 4) / (n - 1) : 0;
      for (int i = 0; i < 4; i++) cv.set(x + i, 7);
      break;
    }
    case UI::Mode::Standby:
      drawSleepy(now);
      break;
  }
  Matrix::setBrightness(md == UI::Mode::Standby ? 0 : bright);
  portENTER_CRITICAL(&fbMux);
  memcpy(shown, cv.col, MTX_W);
  portEXIT_CRITICAL(&fbMux);
  Matrix::show(cv.col);
}
}  // namespace

void UI::begin() {
  loadSettings();
  Matrix::begin();
  Matrix::setBrightness(bright);
  Radio::setVolume(vol);
  setMode(Mode::Boot);
}

void UI::loop() {
  uint32_t now = millis();
  Input::poll();
  Action a;
  while (Input::next(a)) handle(a);

  // network-driven transitions
  Net::State ns = Net::state();
  if (md == Mode::Boot && now - modeSince > 1200) setMode(Mode::Connecting);
  if (md == Mode::Connecting && ns == Net::State::Online) {
    if (standby) setMode(Mode::Standby);
    else { setMode(Mode::Playing); tune(idx); }
  }
  if (ns == Net::State::SetupAP && md != Mode::SetupAP) setMode(Mode::SetupAP);
  if (md == Mode::Browse && now - lastInput > BROWSE_TIMEOUT_MS) setMode(Mode::Playing);

  if (settingsDirty && now - settingsChanged > 3000) saveSettings();

  static uint32_t lastFrame = 0;
  if (now - lastFrame >= 33) { lastFrame = now; render(); }
}

void UI::frame(uint8_t out[MTX_W]) {
  portENTER_CRITICAL(&fbMux);
  memcpy(out, shown, MTX_W);
  portEXIT_CRITICAL(&fbMux);
}
UI::Mode UI::mode() { return md; }
int UI::volume() { return vol; }
int UI::stationIndex() { return idx; }
int UI::brightness() { return bright; }
void UI::setTest(int pattern) { testPat = pattern; }
