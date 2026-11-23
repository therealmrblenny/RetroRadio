// PC simulator for the display layer: runs src/view.cpp through a scripted demo and
// writes every frame as JSON (for sim/preview.html) and optionally ASCII to stdout.
//   ./build.sh && ./sim > frames.json        (see build.sh)
#include <cstdio>
#include <functional>
#include <string>
#include <vector>
#include "../src/view.h"
#include "../src/critters.h"

struct Step { uint32_t t; std::string label; std::function<void(uint32_t)> fn; };

static std::vector<Step> script() {
  using namespace View;
  std::vector<Step> s;
  uint32_t t = 0;
  auto at = [&](uint32_t dt, std::string label, std::function<void(uint32_t)> fn) { t += dt; s.push_back({t, label, fn}); };
  at(0, "Boot", [](uint32_t n) { showBoot(n); });
  at(3100, "Connecting to WiFi", [](uint32_t n) { showConnecting(n); });
  at(3000, "Tune: first station", [](uint32_t n) { setNowPlaying("Groove Salad", "", true, false); tuneTo(n, "Groove Salad", 0, 6); });
  at(1600, "", [](uint32_t) { setNowPlaying("Groove Salad", "", false, false); });
  at(900, "Song title arrives", [](uint32_t) { setNowPlaying("Groove Salad", "Bonobo - Kerala", false, false); });
  at(15000, "Settled on the station name", [](uint32_t) {});
  at(62000, "Long name: one pass a minute", [](uint32_t) {});
  at(8000, "Next song: scrolls again", [](uint32_t) { setNowPlaying("Groove Salad", "Tycho - Awake", false, false); });
  at(9000, "Volume up x4", [](uint32_t n) { showVolume(n, 9, 21); });
  for (int v = 10; v <= 12; v++) at(160, "", [v](uint32_t n) { showVolume(n, v, 21); });
  at(1100, "Volume down x6", [](uint32_t n) { showVolume(n, 11, 21); });
  for (int v = 10; v >= 6; v--) at(120, "", [v](uint32_t n) { showVolume(n, v, 21); });
  at(2400, "Press: browse stations", [](uint32_t n) { showBrowse(n, "1 Groove Salad", 1, 7, 0); });
  at(1200, "Turn", [](uint32_t n) { showBrowse(n, "2 Secret Agent", 2, 7, 1); });
  at(700, "", [](uint32_t n) { showBrowse(n, "3 Triple J Unearthed", 3, 7, 1); });
  at(3400, "Turn back", [](uint32_t n) { showBrowse(n, "2 Secret Agent", 2, 7, -1); });
  at(600, "", [](uint32_t n) { showBrowse(n, "1 Groove Salad", 1, 7, -1); });
  at(600, "Header: open lists", [](uint32_t n) { showBrowse(n, "LISTS", 0, 7, -1, Icon::Back); });
  at(1200, "Press: list picker", [](uint32_t n) { showBrowse(n, "Chill", 0, 3, 0, Icon::Folder); });
  at(900, "", [](uint32_t n) { showBrowse(n, "Aussie", 1, 3, 1, Icon::Folder); });
  at(1100, "Press: stations in list", [](uint32_t n) { showBrowse(n, "1 Double J", 1, 4, 0); });
  at(900, "", [](uint32_t n) { showBrowse(n, "2 ABC Jazz", 2, 4, 1); });
  at(1100, "Press: tune", [](uint32_t n) { setNowPlaying("ABC Jazz", "", true, false); tuneTo(n, "ABC Jazz", 1, 3); });
  at(2600, "", [](uint32_t) { setNowPlaying("ABC Jazz", "Miles Davis - So What", false, false); });
  at(9000, "Stream drops", [](uint32_t) { setNowPlaying("ABC Jazz", "", true, true); });
  // standby runs 40x faster here (acts every ~8-15 s instead of 5-10 min); pokes show each act
  at(6000, "Standby: cat, day (40x speed)", [](uint32_t n) { Critters::setPace(40); setClock(12 * 60); showStandby(n, 0); });
  at(3000, "Poke: moth", [](uint32_t n) { pokeCritter(n); });
  at(30000, "Cat, evening", [](uint32_t) { setClock(19 * 60); });
  at(2000, "Poke: groom", [](uint32_t n) { pokeCritter(n); });
  at(20000, "Cat, night", [](uint32_t) { setClock(23 * 60); });
  at(2000, "Poke: rolls over", [](uint32_t n) { pokeCritter(n); });
  at(8000, "Press: wake", [](uint32_t n) { wake(n); setNowPlaying("ABC Jazz", "", true, false); tuneTo(n, "ABC Jazz", 1, 3); });
  at(3000, "Standby: frog, day (40x speed)", [](uint32_t n) { setClock(12 * 60); showStandby(n, 7); });
  at(3000, "Poke: fly", [](uint32_t n) { pokeCritter(n); });
  at(40000, "Frog, morning", [](uint32_t) { setClock(7 * 60); });
  at(2000, "Poke: yawn", [](uint32_t n) { pokeCritter(n); });
  at(15000, "Frog, evening", [](uint32_t) { setClock(19 * 60); });
  at(2000, "Poke: ribbit", [](uint32_t n) { pokeCritter(n); });
  at(20000, "Frog, night", [](uint32_t) { setClock(23 * 60); });
  at(2000, "Poke: one eye opens", [](uint32_t n) { pokeCritter(n); });
  at(6000, "Press: wake", [](uint32_t n) { wake(n); setNowPlaying("ABC Jazz", "", true, false); tuneTo(n, "ABC Jazz", 1, 3); Critters::setPace(1); });
  at(3200, "No WiFi: setup hotspot", [](uint32_t n) {
    showPages(n, {{"SETUP", Icon::Wifi}, {"JOIN WIFI"}, {"RetroRadio-Setup"}, {"THEN OPEN"}, {"192.168.4.1"}}, true);
  });
  at(19000, "Phone joins the hotspot", [](uint32_t n) {
    showPages(n, {{"CONNECTED", Icon::Wifi}, {"NOW OPEN"}, {"192.168.4.1"}, {"IN A BROWSER"}}, true);
  });
  at(11000, "First time online after setup", [](uint32_t n) {
    showPages(n, {{"ONLINE", Icon::Wifi}, {"OPEN"}, {"retroradio.local"}, {"OR 192.168.0.47"}}, false);
  });
  at(11000, "end", [](uint32_t) {});
  return s;
}

int main(int argc, char **argv) {
  bool ascii = argc > 1 && std::string(argv[1]) == "--ascii";
  auto steps = script();
  uint32_t end = steps.back().t;
  printf("{\"frameMs\":%u,\"runs\":[", View::FRAME_MS);
  const Font fonts[] = {Font::Compact, Font::Classic};
  for (int r = 0; r < 2; r++) {
    View::setFont(fonts[r]);
    View::setUserBrightness(4);
    size_t si = 0;
    std::string frames, bright, marks;
    for (uint32_t now = 0; now < end; now += View::FRAME_MS) {
      while (si < steps.size() && steps[si].t <= now) {
        steps[si].fn(now);
        if (!steps[si].label.empty() && steps[si].label != "end") {
          char m[160]; snprintf(m, sizeof m, "%s{\"t\":%u,\"label\":\"%s\"}", marks.empty() ? "" : ",", now, steps[si].label.c_str());
          marks += m;
        }
        si++;
      }
      Canvas c;
      int b = View::render(now, c);
      char hex[MTX_W * 2 + 4]; char *p = hex;
      *p++ = frames.empty() ? ' ' : ','; *p++ = '"';
      for (int x = 0; x < MTX_W; x++) p += sprintf(p, "%02x", c.col[x]);
      *p++ = '"'; *p = 0;
      frames += hex + (frames.empty() ? 1 : 0);
      bright += (bright.empty() ? "" : ",") + std::to_string(b < 0 ? 4 : b);
      if (ascii && r == 0) {
        fprintf(stderr, "t=%u b=%d\n", now, b);
        for (int y = 0; y < MTX_H; y++) {
          for (int x = 0; x < MTX_W; x++) fputc(c.get(x, y) ? '#' : '.', stderr);
          fputc('\n', stderr);
        }
      }
    }
    printf("%s{\"font\":\"%s\",\"frames\":[%s],\"bright\":[%s],\"marks\":[%s]}", r ? "," : "",
           r == 0 ? "compact" : "classic", frames.c_str(), bright.c_str(), marks.c_str());
  }
  printf("]}\n");
}
