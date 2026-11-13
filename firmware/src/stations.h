#pragma once
#include <Arduino.h>
#include <vector>

struct Station { String name; String url; };

// Station list persisted as /stations.json on LittleFS. Thread-safe.
namespace Stations {
void begin();
size_t count();
bool get(size_t i, Station &out);
String toJson();
bool replaceFromJson(const char *json, size_t len, String &err);
}
