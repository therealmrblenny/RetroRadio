#pragma once
#include <Arduino.h>

// WiFi with fallback setup hotspot, mDNS and ArduinoOTA.
namespace Net {
enum class State : uint8_t { Connecting, Online, SetupAP };
void begin();
void loop();
State state();
String ip();
int rssi();
String ssid();
int apClients();               // phones joined to the setup hotspot
void saveCredentials(const String &ssid, const String &pass);  // then reboot
bool takeJustConfigured();      // true once, the first time we're online after new credentials
String scanJson();              // starts/polls an async scan: {"scanning":true} or {"networks":[...]}
}
