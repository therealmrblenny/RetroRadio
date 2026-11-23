#include "radio.h"
#include "config.h"
#include <WiFi.h>
#include "Audio.h"

namespace {
enum class Cmd : uint8_t { Play, Stop, Volume };
struct Msg { Cmd cmd; uint8_t vol; char name[96]; char url[400]; };

Audio audio;
QueueHandle_t queue;
SemaphoreHandle_t mtx;
Radio::Snapshot state{};
char curUrl[400] = "";
uint32_t lastRetry = 0;

struct Lock { Lock() { xSemaphoreTake(mtx, portMAX_DELAY); } ~Lock() { xSemaphoreGive(mtx); } };

void onAudio(Audio::msg_t m) {
  if (!m.msg) return;
  switch (m.e) {
    case Audio::evt_streamtitle: {
      Lock l; strlcpy(state.title, m.msg, sizeof(state.title));
      Serial.printf("NOW PLAYING: %s\n", m.msg); break;
    }
    case Audio::evt_bitrate: { Lock l; state.bitrate = atoi(m.msg); break; }
    case Audio::evt_vu: case Audio::evt_spectrum: break;
    case Audio::evt_info:
      if (strstr(m.msg, "MP3Decoder")) { Lock l; strlcpy(state.codec, "MP3", sizeof(state.codec)); }
      else if (strstr(m.msg, "AACDecoder")) { Lock l; strlcpy(state.codec, "AAC", sizeof(state.codec)); }
      else if (strstr(m.msg, "FLACDecoder")) { Lock l; strlcpy(state.codec, "FLAC", sizeof(state.codec)); }
      else if (strstr(m.msg, "OPUSDecoder")) { Lock l; strlcpy(state.codec, "OPUS", sizeof(state.codec)); }
      Serial.printf("[audio] %s\n", m.msg); break;
    default: Serial.printf("[%s] %s\n", m.s ? m.s : "?", m.msg); break;
  }
}

void startStream() {
  { Lock l; state.title[0] = 0; state.codec[0] = 0; state.bitrate = 0; state.sinceMs = millis(); }
  lastRetry = millis();
  audio.connecttohost(curUrl);
}

void task(void *) {
  for (;;) {
    Msg m;
    while (xQueueReceive(queue, &m, 0) == pdTRUE) {
      switch (m.cmd) {
        case Cmd::Play:
          strlcpy(curUrl, m.url, sizeof(curUrl));
          { Lock l; strlcpy(state.name, m.name, sizeof(state.name)); state.wantPlaying = true; }
          startStream();
          break;
        case Cmd::Stop:
          audio.stopSong();
          { Lock l; state.wantPlaying = false; state.title[0] = 0; }
          break;
        case Cmd::Volume: audio.setVolume(m.vol); break;
      }
    }
    static uint32_t lastT = micros(), winStart = 0, gapMax = 0, loopMax = 0, bufMin = UINT32_MAX;
    uint32_t t0 = micros();
    gapMax = max(gapMax, t0 - lastT);
    audio.loop();
    uint32_t t1 = micros();
    loopMax = max(loopMax, t1 - t0);
    lastT = t1;
    uint32_t fill = audio.inBufferFilled();
    bufMin = min(bufMin, fill);
    bool running = audio.isRunning();
    bool want;
    { Lock l; state.running = running; want = state.wantPlaying;
      state.bufFill = fill; state.bufSize = audio.getInBufferSize();
      if (millis() - winStart > 2000) {
        state.bufMin = bufMin; state.gapMaxUs = gapMax; state.loopMaxUs = loopMax;
        winStart = millis(); gapMax = loopMax = 0; bufMin = UINT32_MAX;
      } }
    // Auto-reconnect: dropped streams or failed connects retry every 10s
    if (want && !running && WiFi.isConnected() && millis() - lastRetry > 10000) {
      Serial.println("[radio] stream not running - retrying");
      startStream();
    }
    vTaskDelay(1);
  }
}

void send(const Msg &m) { xQueueSend(queue, &m, pdMS_TO_TICKS(100)); }
}  // namespace

void Radio::begin() {
  mtx = xSemaphoreCreateMutex();
  queue = xQueueCreate(4, sizeof(Msg));
  Audio::audio_info_callback = onAudio;
  // HLS streams (ABC) download each ~10 s segment in a burst; with the decode task on
  // core 0 next to WiFi/lwIP it gets starved and the ~90 ms of I2S DMA runs dry.
  // More DMA headroom (24 x 256 frames ~ 130 ms) + decode alone on core 1 (see main.cpp).
  audio.settings.DMA_DESC_NUM = 24;
  audio.setPinout(PIN_I2S_BCK, PIN_I2S_WS, PIN_I2S_DOUT);
  audio.setAudioTaskCore(1);
  audio.setVolumeSteps(VOL_MAX);
  audio.setConnectionTimeout(5000, 8000);
  // stream fetch + TLS: core 0, below the UI so a download burst can't stall the screen
  xTaskCreatePinnedToCore(task, "radio", 12288, nullptr, 3, nullptr, 0);
}

void Radio::play(const char *name, const char *url) {
  Msg m{}; m.cmd = Cmd::Play;
  strlcpy(m.name, name, sizeof(m.name)); strlcpy(m.url, url, sizeof(m.url));
  send(m);
}
void Radio::stop() { Msg m{}; m.cmd = Cmd::Stop; send(m); }
void Radio::setVolume(uint8_t v) { Msg m{}; m.cmd = Cmd::Volume; m.vol = min<uint8_t>(v, VOL_MAX); send(m); }

Radio::Snapshot Radio::snapshot() { Lock l; return state; }
