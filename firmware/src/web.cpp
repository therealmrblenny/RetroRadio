#include "web.h"
#include "web_ui.h"
#include "ui.h"
#include "radio.h"
#include "stations.h"
#include "input.h"
#include "net.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

#ifndef FW_VERSION
#define FW_VERSION __DATE__ " " __TIME__
#endif

namespace {
AsyncWebServer server(80);
uint32_t restartAt = 0;

const char *modeName(UI::Mode m) {
  switch (m) {
    case UI::Mode::Boot: return "boot";
    case UI::Mode::Connecting: return "connecting";
    case UI::Mode::SetupAP: return "setup";
    case UI::Mode::Playing: return "playing";
    case UI::Mode::Browse: return "browse";
    case UI::Mode::Standby: return "standby";
  }
  return "?";
}

void sendState(AsyncWebServerRequest *req) {
  auto s = Radio::snapshot();
  uint8_t fb[MTX_W]; UI::frame(fb);
  char hex[MTX_W * 2 + 1];
  for (int i = 0; i < MTX_W; i++) sprintf(hex + i * 2, "%02x", fb[i]);
  JsonDocument d;
  d["mode"] = modeName(UI::mode());
  d["vol"] = UI::volume(); d["volMax"] = VOL_MAX;
  d["idx"] = UI::stationIndex(); d["bright"] = UI::brightness();
  d["name"] = s.name; d["title"] = s.title; d["codec"] = s.codec; d["bitrate"] = s.bitrate;
  d["playing"] = s.wantPlaying; d["running"] = s.running;
  d["net"] = Net::state() == Net::State::Online ? "online" : Net::state() == Net::State::SetupAP ? "setup hotspot" : "connecting";
  d["ip"] = Net::ip(); d["rssi"] = Net::rssi(); d["ssid"] = Net::ssid();
  d["heap"] = ESP.getFreeHeap(); d["psram"] = ESP.getFreePsram(); d["uptime"] = millis() / 1000;
  d["fw"] = FW_VERSION; d["fb"] = hex;
  String out; serializeJson(d, out);
  req->send(200, "application/json", out);
}

void handleCmd(AsyncWebServerRequest *req) {
  String c = req->hasParam("c") ? req->getParam("c")->value() : "";
  int v = req->hasParam("v") ? req->getParam("v")->value().toInt() : 0;
  String sv = req->hasParam("v") ? req->getParam("v")->value() : "";
  if (c == "vol") Input::post(Act::SetVolume, v);
  else if (c == "play") Input::post(Act::Play, v);
  else if (c == "next") Input::post(Act::Next);
  else if (c == "prev") Input::post(Act::Prev);
  else if (c == "standby") Input::post(Act::Standby, v);
  else if (c == "bright") Input::post(Act::SetBrightness, v);
  else if (c == "key") {
    if (sv == "cw") Input::post(Act::TurnCW);
    else if (sv == "ccw") Input::post(Act::TurnCCW);
    else if (sv == "press") Input::post(Act::Press);
    else if (sv == "long") Input::post(Act::LongPress);
    else { req->send(400, "text/plain", "unknown key"); return; }
  } else { req->send(400, "text/plain", "unknown command"); return; }
  req->send(204);
}
}  // namespace

void Web::begin() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *r) { r->send(200, "text/html", INDEX_HTML); });
  server.on("/api/state", HTTP_GET, sendState);
  server.on("/api/cmd", HTTP_POST, handleCmd);
  server.on("/api/stations", HTTP_GET, [](AsyncWebServerRequest *r) {
    r->send(200, "application/json", Stations::toJson());
  });
  // body arrives in chunks; collect into a String attached to the request
  server.on("/api/stations", HTTP_POST,
    [](AsyncWebServerRequest *r) {
      String *body = (String *)r->_tempObject;
      String err;
      if (!body) { r->send(400, "text/plain", "empty body"); return; }
      bool ok = Stations::replaceFromJson(body->c_str(), body->length(), err);
      delete body; r->_tempObject = nullptr;
      if (!ok) { r->send(400, "text/plain", err); return; }
      Input::post(Act::StationsChanged);
      r->send(204);
    },
    nullptr,
    [](AsyncWebServerRequest *r, uint8_t *data, size_t len, size_t index, size_t total) {
      if (total > 48 * 1024) return;
      if (index == 0) { r->_tempObject = new String(); ((String *)r->_tempObject)->reserve(total); }
      ((String *)r->_tempObject)->concat((const char *)data, len);
    });
  server.on("/api/wifi", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!r->hasParam("ssid", true)) { r->send(400, "text/plain", "ssid required"); return; }
    Net::saveCredentials(r->getParam("ssid", true)->value(),
                         r->hasParam("pass", true) ? r->getParam("pass", true)->value() : "");
    r->send(204);
    restartAt = millis() + 1500;
  });
  // captive portal / unknown paths -> the main page
  server.onNotFound([](AsyncWebServerRequest *r) { r->redirect("http://" + Net::ip() + "/"); });
  server.begin();
}

void Web::loop() {
  if (restartAt && (int32_t)(millis() - restartAt) > 0) ESP.restart();
}
