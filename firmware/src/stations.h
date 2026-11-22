#pragma once
#include <Arduino.h>
#include <vector>

// disp = optional short name for the LED screen (empty = use name)
struct Station {
  String name; String url; String disp;
  const String &shown() const { return disp.isEmpty() ? name : disp; }
};

// Stations grouped into named lists, persisted as /stations.json on LittleFS:
//   {"lists":[{"name":"Favourites","stations":[{"name":..,"url":..,"disp":..}, ...]}, ...]}
// The old flat-array format is still accepted and becomes one list. Thread-safe.
namespace Stations {
constexpr size_t MAX_STATIONS = 300, MAX_LISTS = 20;
void begin();
size_t lists();
String listName(size_t l);
size_t count(size_t l);
bool get(size_t l, size_t i, Station &out);
String toJson();
bool replaceFromJson(const char *json, size_t len, String &err);
}
