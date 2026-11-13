#include "stations.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

namespace {
std::vector<Station> list;
SemaphoreHandle_t mtx;
const char *PATH = "/stations.json";

const Station DEFAULTS[] = {
  {"Groove Salad", "http://ice1.somafm.com/groovesalad-128-mp3"},
  {"Secret Agent", "http://ice1.somafm.com/secretagent-128-mp3"},
  {"Drone Zone",   "http://ice1.somafm.com/dronezone-128-mp3"},
  {"Left Coast 70s", "http://ice1.somafm.com/seventies-128-mp3"},
};

struct Lock { Lock() { xSemaphoreTake(mtx, portMAX_DELAY); } ~Lock() { xSemaphoreGive(mtx); } };

bool parse(const char *json, size_t len, std::vector<Station> &out, String &err) {
  JsonDocument doc;
  auto e = deserializeJson(doc, json, len);
  if (e) { err = e.c_str(); return false; }
  if (!doc.is<JsonArray>()) { err = "expected an array"; return false; }
  for (JsonObject o : doc.as<JsonArray>()) {
    String name = o["name"] | "", url = o["url"] | "";
    name.trim(); url.trim();
    if (!url.startsWith("http://") && !url.startsWith("https://")) { err = "bad url: " + url; return false; }
    if (name.isEmpty()) name = url;
    out.push_back({name, url});
  }
  if (out.size() > 200) { err = "too many stations (max 200)"; return false; }
  return true;
}

String serialize(const std::vector<Station> &v) {
  JsonDocument doc;
  JsonArray a = doc.to<JsonArray>();
  for (auto &s : v) { JsonObject o = a.add<JsonObject>(); o["name"] = s.name; o["url"] = s.url; }
  String out; serializeJson(doc, out); return out;
}

void save() {
  File f = LittleFS.open(PATH, "w");
  if (f) { f.print(serialize(list)); f.close(); }
}
}  // namespace

void Stations::begin() {
  mtx = xSemaphoreCreateMutex();
  if (!LittleFS.begin(true)) Serial.println("LittleFS mount failed");
  Lock l;
  File f = LittleFS.open(PATH, "r");
  if (f) {
    String s = f.readString(); f.close();
    String err;
    if (!parse(s.c_str(), s.length(), list, err)) { Serial.printf("stations.json: %s\n", err.c_str()); list.clear(); }
  }
  if (list.empty()) {
    for (auto &d : DEFAULTS) list.push_back(d);
    save();
  }
  Serial.printf("Stations: %u loaded\n", (unsigned)list.size());
}

size_t Stations::count() { Lock l; return list.size(); }

bool Stations::get(size_t i, Station &out) {
  Lock l;
  if (i >= list.size()) return false;
  out = list[i]; return true;
}

String Stations::toJson() { Lock l; return serialize(list); }

bool Stations::replaceFromJson(const char *json, size_t len, String &err) {
  std::vector<Station> v;
  if (!parse(json, len, v, err)) return false;
  if (v.empty()) { err = "list cannot be empty"; return false; }
  Lock l; list = std::move(v); save();
  return true;
}
