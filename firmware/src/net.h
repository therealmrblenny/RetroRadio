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
void saveCredentials(const String &ssid, const String &pass);  // then reboot
}
