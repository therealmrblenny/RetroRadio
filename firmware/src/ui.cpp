#include "ui.h"
#include "view.h"
#include "canvas.h"
#include "matrix.h"
#include "input.h"
#include "radio.h"
#include "stations.h"
#include "net.h"
#include <Preferences.h>
#include <time.h>

namespace {
using Mode = UI::Mode;
Mode md = Mode::Boot;
uint8_t shown[MTX_W];
portMUX_TYPE fbMux = portMUX_INITIALIZER_UNLOCKED;

// persisted settings
int vol = 8, list = 0, idx = 0, bright = 3;
bool standby = false, compact = true;
bool settingsDirty = false;
uint32_t settingsChanged = 0;

int testPat = 0;            // 0 = off; see drawTest()
uint32_t lastInput = 0;
int browseList = 0, browseSel = 0;   // browseSel -1 = the "LISTS" entry (only with 2+ lists)
int listSel = 0;
int apClients = -1;
// a station being auditioned from the web search (not in any list)
portMUX_TYPE tryMux = portMUX_INITIALIZER_UNLOCKED;
char tryName[96], tryUrl[400];

void markSettings() { settingsDirty = true; settingsChanged = millis(); }

void loadSettings() {
  Preferences p; p.begin("radio", true);
  vol = p.getInt("vol", 8); list = p.getInt("list", 0); idx = p.getInt("idx", 0);
  bright = p.getInt("bright", 3); standby = p.getBool("standby", false);
  compact = p.getBool("compact", true);
  p.end();
}
void saveSettings() {
  Preferences p; p.begin("radio", false);
  p.putInt("vol", vol); p.putInt("list", list); p.putInt("idx", idx); p.putInt("bright", bright);
  p.putBool("standby", standby); p.putBool("compact", compact);
  p.end();
  settingsDirty = false;
}

void setMode(Mode m) { md = m; }
bool multiList() { return Stations::lists() > 1; }

void clampStation() {
  if (list >= (int)Stations::lists()) list = 0;
  if (idx >= (int)Stations::count(list)) idx = 0;
}

void tune(int l, int i) {
  if (l < 0 || l >= (int)Stations::lists()) l = 0;
  int n = Stations::count(l);
  if (!n) return;
  list = l; idx = ((i % n) + n) % n;
  Station s;
  if (Stations::get(list, idx, s)) {
    Radio::play(s.name.c_str(), s.url.c_str());
    View::tuneTo(millis(), s.name.c_str(), idx, n);
  }
  setMode(Mode::Playing);
  markSettings();
}

void enterStandby() {
  standby = true; markSettings();
  Radio::stop();
  View::showStandby(millis(), esp_random());
  setMode(Mode::Standby);
}
void wake() {
  standby = false; markSettings();
  View::wake(millis());
  tune(list, idx);
}
void changeVolume(int v) {
  vol = constrain(v, 0, VOL_MAX);
  Radio::setVolume(vol);
  View::showVolume(millis(), vol, VOL_MAX);
  markSettings();
}

// ---------- browse / lists ----------
void showBrowseItem(int dir) {
  uint32_t now = millis();
  int n = Stations::count(browseList);
  int pos = browseSel + (multiList() ? 1 : 0), total = n + (multiList() ? 1 : 0);
  if (browseSel < 0) { View::showBrowse(now, "LISTS", pos, total, dir, View::Icon::Back); return; }
  Station st; Stations::get(browseList, browseSel, st);
  View::showBrowse(now, (String(browseSel + 1) + " " + st.name).c_str(), pos, total, dir);
}
void startBrowse(int l, int sel) {
  browseList = l; browseSel = sel;
  setMode(Mode::Browse);
  showBrowseItem(0);
}
void stepBrowse(int d) {
  int n = Stations::count(browseList);
  int lo = multiList() ? -1 : 0;              // the LISTS entry sits between the last and first station
  int span = n - lo;
  browseSel = ((browseSel - lo + d) % span + span) % span + lo;
  showBrowseItem(d);
}
void showListItem(int dir) {
  String label = Stations::listName(listSel);
  if (!Stations::count(listSel)) label += " (EMPTY)";
  View::showBrowse(millis(), label.c_str(), listSel, Stations::lists(), dir, View::Icon::Folder);
}
void backToPlaying() { setMode(Mode::Playing); View::showPlaying(millis()); }

// ---------- setup hotspot / first-connect screens ----------
void showSetupPages(int clients) {
  std::vector<View::Page> p;
  String ip = Net::ip();
  if (clients > 0) p = {{"CONNECTED", View::Icon::Wifi}, {"NOW OPEN"}, {ip.c_str()}, {"IN A BROWSER"}};
  else p = {{"SETUP", View::Icon::Wifi}, {"JOIN WIFI"}, {AP_SSID}, {"THEN OPEN"}, {ip.c_str()}};
  View::showPages(millis(), p, true);
}

void startPlayback() {
  if (standby) { View::showStandby(millis(), esp_random()); setMode(Mode::Standby); }
  else tune(list, idx);
}

// ---------- input ----------
void handle(const Action &a) {
  lastInput = millis();
  using A = Act;
  // direct commands work in any mode
  switch (a.type) {
    case A::SetVolume: if (md != Mode::Standby) changeVolume(a.v); return;
    case A::Play:
      if (md == Mode::Standby) { standby = false; View::wake(millis()); }
      tune(a.v >> 16, a.v & 0xFFFF); return;
    case A::Next: if (md == Mode::Playing) tune(list, idx + 1); return;
    case A::Prev: if (md == Mode::Playing) tune(list, idx - 1); return;
    case A::Standby:
      if (a.v && md != Mode::Standby) enterStandby();
      else if (!a.v && md == Mode::Standby) wake();
      return;
    case A::SetBrightness: bright = constrain(a.v, 0, 7); markSettings(); return;
    case A::SetFont:
      compact = a.v != 0; View::setFont(compact ? Font::Compact : Font::Classic); markSettings(); return;
    case A::PlayUrl: {
      char n[sizeof tryName], u[sizeof tryUrl];
      portENTER_CRITICAL(&tryMux);
      memcpy(n, tryName, sizeof n); memcpy(u, tryUrl, sizeof u);
      portEXIT_CRITICAL(&tryMux);
      if (md == Mode::Standby) { standby = false; markSettings(); View::wake(millis()); }
      Radio::play(n, u);
      View::tuneTo(millis(), n, 0, 1);
      setMode(Mode::Playing);
      return;
    }
    case A::StationsChanged:
      clampStation();
      if (md == Mode::Browse || md == Mode::Lists) backToPlaying();
      return;
    default: break;
  }
  // knob
  switch (md) {
    case Mode::Playing:
      if (a.type == A::TurnCW) changeVolume(vol + 1);
      else if (a.type == A::TurnCCW) changeVolume(vol - 1);
      else if (a.type == A::Press) startBrowse(list, idx);
      else if (a.type == A::LongPress) enterStandby();
      break;
    case Mode::Browse:
      if (a.type == A::TurnCW) stepBrowse(1);
      else if (a.type == A::TurnCCW) stepBrowse(-1);
      else if (a.type == A::Press) {
        if (browseSel < 0) { listSel = browseList; setMode(Mode::Lists); showListItem(0); }
        else if (browseList == list && browseSel == idx && Radio::snapshot().wantPlaying) backToPlaying();
        else tune(browseList, browseSel);
      } else if (a.type == A::LongPress) backToPlaying();
      break;
    case Mode::Lists: {
      int n = Stations::lists();
      if (a.type == A::TurnCW) { listSel = (listSel + 1) % n; showListItem(1); }
      else if (a.type == A::TurnCCW) { listSel = (listSel - 1 + n) % n; showListItem(-1); }
      else if (a.type == A::Press && Stations::count(listSel)) startBrowse(listSel, listSel == list ? idx : 0);
      else if (a.type == A::LongPress) backToPlaying();
      break;
    }
    case Mode::Standby:
      if (a.type == A::Press || a.type == A::LongPress) wake();
      break;
    default: break;
  }
}

// ---------- now-playing info for the view ----------
void feedNowPlaying(uint32_t now) {
  auto s = Radio::snapshot();
  bool buffering = s.wantPlaying && !s.running;
  bool stalled = buffering && now - s.sinceMs > 12000;
  View::setNowPlaying(s.name, s.title, buffering, stalled);
}

// Wiring test patterns (web: /api/cmd?c=test&v=N, 0 = off)
//  1 orientation: solid 3x3 block top-left, "L" left half, "R" right half
//  2 column walker, 3 row walker, 4 all pixels on
void drawTest(Canvas &cv, uint32_t now) {
  switch (testPat) {
    case 1:
      for (int x = 0; x < 3; x++) for (int y = 0; y < 3; y++) cv.set(x, y);
      cv.text(6, "L"); cv.text(22, "R");
      break;
    case 2: { int x = (now / 250) % MTX_PHYS_W; cv.column(MTX_GAP_COL >= 0 && x >= MTX_GAP_COL ? x + 1 : x, 0xFF); break; }
    case 3: for (int x = 0; x < MTX_W; x++) cv.set(x, (now / 400) % MTX_H); break;
    case 4: for (int x = 0; x < MTX_W; x++) cv.column(x, 0xFF); break;
  }
}

void render(uint32_t now) {
  Canvas cv; cv.clear();
  int b = bright;
  if (testPat) drawTest(cv, now);
  else {
    View::setUserBrightness(bright);
    int o = View::render(now, cv);
    if (o >= 0) b = o;
  }
  Matrix::setBrightness(b);
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
  View::setFont(compact ? Font::Compact : Font::Classic);
  View::setUserBrightness(bright);
  View::showBoot(millis());
  setMode(Mode::Boot);
}

void UI::loop() {
  uint32_t now = millis();
  Input::poll();
  Action a;
  while (Input::next(a)) handle(a);

  Net::State ns = Net::state();
  switch (md) {
    case Mode::Boot:
      if (View::bootDone(now)) {
        if (ns == Net::State::Online) startPlayback();
        else { setMode(Mode::Connecting); View::showConnecting(now); }
      }
      break;
    case Mode::Connecting:
      if (ns == Net::State::Online) {
        if (Net::takeJustConfigured()) {
          setMode(Mode::Notice);
          View::showPages(now, {{"ONLINE", View::Icon::Wifi}, {"OPEN"}, {HOSTNAME ".local"}, {("OR " + Net::ip()).c_str()}}, false);
        } else startPlayback();
      }
      break;
    case Mode::Notice:
      if (View::pagesDone(now)) startPlayback();
      break;
    case Mode::SetupAP: {
      int c = Net::apClients();
      if ((c > 0) != (apClients > 0)) showSetupPages(c);
      apClients = c;
      break;
    }
    case Mode::Browse: case Mode::Lists:
      if (now - lastInput > BROWSE_TIMEOUT_MS) backToPlaying();
      break;
    default: break;
  }
  if (ns == Net::State::SetupAP && md != Mode::SetupAP && md != Mode::Boot) {
    setMode(Mode::SetupAP); apClients = Net::apClients(); showSetupPages(apClients);
  }
  if (md == Mode::Playing || md == Mode::Browse || md == Mode::Lists) feedNowPlaying(now);

  if (settingsDirty && now - settingsChanged > 3000) saveSettings();

  static uint32_t lastClock = 0;          // local time (from NTP) for the standby scenes
  if (now - lastClock > 5000) {
    lastClock = now;
    struct tm tm;
    View::setClock(getLocalTime(&tm, 0) && tm.tm_year > 120 ? tm.tm_hour * 60 + tm.tm_min : -1);
  }

  // fixed frame cadence so scrolling moves exactly 1 px per frame
  static uint32_t nextFrame = 0;
  if ((int32_t)(now - nextFrame) >= 0) {
    nextFrame += View::FRAME_MS;
    if ((int32_t)(now - nextFrame) > 200) nextFrame = now + View::FRAME_MS;   // fell behind: resync
    render(now);
  }
}

void UI::frame(uint8_t out[MTX_W]) {
  portENTER_CRITICAL(&fbMux);
  memcpy(out, shown, MTX_W);
  portEXIT_CRITICAL(&fbMux);
}
UI::Mode UI::mode() { return md; }
const char *UI::modeName() {
  switch (md) {
    case Mode::Boot: return "boot";
    case Mode::Connecting: return "connecting";
    case Mode::SetupAP: return "setup";
    case Mode::Notice: return "notice";
    case Mode::Playing: return "playing";
    case Mode::Browse: return "browse";
    case Mode::Lists: return "lists";
    case Mode::Standby: return "standby";
  }
  return "?";
}
int UI::volume() { return vol; }
int UI::listIndex() { return list; }
int UI::stationIndex() { return idx; }
int UI::brightness() { return bright; }
bool UI::compactFont() { return compact; }
void UI::setTest(int pattern) { testPat = pattern; }
void UI::tryStation(const String &name, const String &url) {
  char n[sizeof tryName], u[sizeof tryUrl];
  strlcpy(n, name.c_str(), sizeof n); strlcpy(u, url.c_str(), sizeof u);
  portENTER_CRITICAL(&tryMux);
  memcpy(tryName, n, sizeof n); memcpy(tryUrl, u, sizeof u);
  portEXIT_CRITICAL(&tryMux);
  Input::post(Act::PlayUrl);
}
