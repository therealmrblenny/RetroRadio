// RetroRadio - internet radio for the Sonocotta HiFi-ESP32
// 2x XC3746 LED matrix (32x8 marquee) + SR1230 rotary encoder, line out to an amp.
#include <Arduino.h>
#include "config.h"
#include "input.h"
#include "stations.h"
#include "radio.h"
#include "net.h"
#include "ui.h"
#include "web.h"

// Serial test keys (no knob needed): + - = turn, p = press, l = hold, s = status
static void serialKeys() {
  while (Serial.available()) {
    switch (Serial.read()) {
      case '+': Input::post(Act::TurnCW); break;
      case '-': Input::post(Act::TurnCCW); break;
      case 'p': Input::post(Act::Press); break;
      case 'l': Input::post(Act::LongPress); break;
      case 's': {
        auto s = Radio::snapshot();
        Serial.printf("mode=%d vol=%d idx=%d name='%s' title='%s' running=%d heap=%u psram=%u\n",
                      (int)UI::mode(), UI::volume(), UI::stationIndex(), s.name, s.title, s.running,
                      ESP.getFreeHeap(), ESP.getFreePsram());
        break;
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n=== RetroRadio ===");
  Input::begin();
  Stations::begin();
  Radio::begin();
  UI::begin();
  Net::begin();
  Web::begin();
}

void loop() {
  Net::loop();
  UI::loop();
  Web::loop();
  serialKeys();
  delay(2);
}
