#include "net.h"
#include "config.h"
#include "secrets.h"
#include "radio.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>

namespace {
Net::State st = Net::State::Connecting;
String wifiSsid, wifiPass;
uint32_t attemptStart = 0, connectStart = 0, apStart = 0;
bool servicesUp = false;
DNSServer dns;

void loadCreds() {
  Preferences p; p.begin("wifi", true);
  wifiSsid = p.getString("ssid", WIFI_SSID);
  wifiPass = p.getString("pass", WIFI_PASS);
  p.end();
}

void beginSTA() {
  WiFi.disconnect();
  WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());
  attemptStart = millis();
}

void startServices() {
  if (servicesUp) return;
  servicesUp = true;
  configTzTime(TZ_INFO, "pool.ntp.org", "time.google.com");   // clock for the standby scenes
  MDNS.begin(HOSTNAME);
  MDNS.addService("http", "tcp", 80);
  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASS);
  ArduinoOTA.onStart([] { Serial.println("OTA start - stopping audio"); Radio::stop(); });
  ArduinoOTA.onEnd([] { Serial.println("OTA done - rebooting"); });
  ArduinoOTA.onError([](ota_error_t e) { Serial.printf("OTA error %u\n", e); });
  ArduinoOTA.begin();
}

void startAP() {
  Serial.printf("WiFi: can't join '%s' - starting setup hotspot '%s'\n", wifiSsid.c_str(), AP_SSID);
  WiFi.disconnect();
  WiFi.mode(WIFI_AP_STA);              // STA side stays idle but lets the setup page scan
  WiFi.softAP(AP_SSID);
  dns.start(53, "*", WiFi.softAPIP());   // captive portal: every hostname -> us
  apStart = millis();
  st = Net::State::SetupAP;
}
}  // namespace

void Net::begin() {
  loadCreds();
  WiFi.persistent(false);
  WiFi.setHostname(HOSTNAME);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);              // lower latency, steadier streaming
  WiFi.setAutoReconnect(true);
  Serial.printf("WiFi: joining '%s'\n", wifiSsid.c_str());
  beginSTA();
  connectStart = millis();
}

void Net::loop() {
  uint32_t now = millis();
  switch (st) {
    case State::Connecting:
      if (WiFi.status() == WL_CONNECTED) {
        st = State::Online;
        Serial.printf("WiFi OK  IP=%s  RSSI=%d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
        startServices();
      } else if (now - connectStart > 60000) {
        startAP();
      } else if (now - attemptStart > 15000) {
        // routers sometimes refuse quick re-joins; start a fresh attempt
        Serial.println("WiFi: retrying");
        beginSTA();
      }
      break;
    case State::Online:
      ArduinoOTA.handle();
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi lost - reconnecting");
        st = State::Connecting; connectStart = now; beginSTA();
      }
      break;
    case State::SetupAP:
      dns.processNextRequest();
      // if the router was just down, try again every 5 minutes (reboot = clean retry)
      if (now - apStart > 5 * 60000 && WiFi.softAPgetStationNum() == 0) ESP.restart();
      break;
  }
}

Net::State Net::state() { return st; }
String Net::ip() { return st == State::SetupAP ? WiFi.softAPIP().toString() : WiFi.localIP().toString(); }
int Net::rssi() { return st == State::Online ? WiFi.RSSI() : 0; }
String Net::ssid() { return wifiSsid; }
int Net::apClients() { return st == State::SetupAP ? WiFi.softAPgetStationNum() : 0; }

void Net::saveCredentials(const String &s, const String &p) {
  Preferences pr; pr.begin("wifi", false);
  pr.putString("ssid", s); pr.putString("pass", p); pr.putBool("fresh", true);
  pr.end();
}

bool Net::takeJustConfigured() {
  Preferences p; p.begin("wifi", false);
  bool f = p.getBool("fresh", false);
  if (f) p.putBool("fresh", false);
  p.end();
  return f;
}

String Net::scanJson() {
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_FAILED) { WiFi.scanNetworks(true); return "{\"scanning\":true}"; }
  if (n == WIFI_SCAN_RUNNING) return "{\"scanning\":true}";
  JsonDocument d;
  JsonArray a = d["networks"].to<JsonArray>();
  for (int i = 0; i < n; i++) {
    String s = WiFi.SSID(i);
    if (s.isEmpty()) continue;
    bool dup = false;
    for (JsonObject o : a) if (s == (const char *)o["ssid"]) { dup = true; break; }
    if (dup) continue;
    JsonObject o = a.add<JsonObject>();
    o["ssid"] = s; o["rssi"] = WiFi.RSSI(i); o["open"] = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
  }
  WiFi.scanDelete();
  String out; serializeJson(d, out);
  return out;
}
