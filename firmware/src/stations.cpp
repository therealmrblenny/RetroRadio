#include "stations.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

namespace {
struct List { String name; std::vector<Station> st; };
std::vector<List> db;
SemaphoreHandle_t mtx;
const char *PATH = "/stations.json";

const Station DEFAULTS[] = {
  {"Groove Salad", "http://ice1.somafm.com/groovesalad-128-mp3"},
  {"Secret Agent", "http://ice1.somafm.com/secretagent-128-mp3"},
  {"Drone Zone",   "http://ice1.somafm.com/dronezone-128-mp3"},
  {"Left Coast 70s", "http://ice1.somafm.com/seventies-128-mp3"},
};

struct Lock { Lock() { xSemaphoreTake(mtx, portMAX_DELAY); } ~Lock() { xSemaphoreGive(mtx); } };

bool parseStations(JsonArrayConst a, std::vector<Station> &out, String &err) {
  for (JsonObjectConst o : a) {
    String name = o["name"] | "", url = o["url"] | "";
    name.trim(); url.trim();
    if (!url.startsWith("http://") && !url.startsWith("https://")) { err = "bad url: " + url; return false; }
    if (name.isEmpty()) name = url;
    out.push_back({name, url});
  }
  return true;
}

bool parse(const char *json, size_t len, std::vector<List> &out, String &err) {
  JsonDocument doc;
  auto e = deserializeJson(doc, json, len);
  if (e) { err = e.c_str(); return false; }
  if (doc.is<JsonArray>()) {                         // legacy: one flat list
    List l{"Stations", {}};
    if (!parseStations(doc.as<JsonArrayConst>(), l.st, err)) return false;
    out.push_back(std::move(l));
  } else if (doc["lists"].is<JsonArray>()) {
    for (JsonObjectConst o : doc["lists"].as<JsonArrayConst>()) {
      List l; l.name = o["name"] | ""; l.name.trim();
      if (l.name.isEmpty()) l.name = "List " + String(out.size() + 1);
      if (!parseStations(o["stations"].as<JsonArrayConst>(), l.st, err)) return false;
      out.push_back(std::move(l));
    }
  } else { err = "expected {\"lists\":[...]}"; return false; }
  size_t total = 0;
  for (auto &l : out) total += l.st.size();
  if (out.empty() || total == 0) { err = "need at least one station"; return false; }
  if (out.size() > Stations::MAX_LISTS) { err = "too many lists (max 20)"; return false; }
  if (total > Stations::MAX_STATIONS) { err = "too many stations (max 300)"; return false; }
  return true;
}

String serialize(const std::vector<List> &v) {
  JsonDocument doc;
  JsonArray la = doc["lists"].to<JsonArray>();
  for (auto &l : v) {
    JsonObject lo = la.add<JsonObject>();
    lo["name"] = l.name;
    JsonArray a = lo["stations"].to<JsonArray>();
    for (auto &s : l.st) { JsonObject o = a.add<JsonObject>(); o["name"] = s.name; o["url"] = s.url; }
  }
  String out; serializeJson(doc, out); return out;
}

void save() {
  File f = LittleFS.open(PATH, "w");
  if (f) { f.print(serialize(db)); f.close(); }
}
}  // namespace

void Stations::begin() {
  mtx = xSemaphoreCreateMutex();
  if (!LittleFS.begin(true)) Serial.println("LittleFS mount failed");
  Lock l;
  File f = LittleFS.open(PATH, "r");
  bool legacy = false;
  if (f) {
    String s = f.readString(); f.close();
    String err;
    legacy = s.startsWith("[");
    if (!parse(s.c_str(), s.length(), db, err)) { Serial.printf("stations.json: %s\n", err.c_str()); db.clear(); }
  }
  if (db.empty()) {
    db.push_back({"Stations", {}});
    for (auto &d : DEFAULTS) db[0].st.push_back(d);
    save();
  } else if (legacy) save();   // rewrite in the new format
  Serial.printf("Stations: %u lists\n", (unsigned)db.size());
}

size_t Stations::lists() { Lock l; return db.size(); }
String Stations::listName(size_t i) { Lock l; return i < db.size() ? db[i].name : String(); }
size_t Stations::count(size_t i) { Lock l; return i < db.size() ? db[i].st.size() : 0; }

bool Stations::get(size_t li, size_t i, Station &out) {
  Lock l;
  if (li >= db.size() || i >= db[li].st.size()) return false;
  out = db[li].st[i]; return true;
}

String Stations::toJson() { Lock l; return serialize(db); }

bool Stations::replaceFromJson(const char *json, size_t len, String &err) {
  std::vector<List> v;
  if (!parse(json, len, v, err)) return false;
  Lock l; db = std::move(v); save();
  return true;
}
