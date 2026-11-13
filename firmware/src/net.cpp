#include "net.h"
#include "config.h"
#include "secrets.h"
#include "radio.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <Preferences.h>

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
  WiFi.mode(WIFI_AP);
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

void Net::saveCredentials(const String &s, const String &p) {
  Preferences pr; pr.begin("wifi", false);
  pr.putString("ssid", s); pr.putString("pass", p);
  pr.end();
}
