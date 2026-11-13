#include "input.h"
#include "config.h"

namespace {
QueueHandle_t queue;
volatile int32_t encTicks = 0;
volatile uint8_t encPrev = 0;
int32_t encUsed = 0;

// Quadrature transition table: index = (prev<<2)|cur -> -1, 0, +1
const int8_t QDEC[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};

void IRAM_ATTR encISR() {
  uint8_t cur = (digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B);
  encTicks += QDEC[(encPrev << 2) | cur];
  encPrev = cur;
}

// button debounce / long-press
bool btnStable = false, btnLastRaw = false, longFired = false;
uint32_t btnChange = 0, btnDown = 0;
}  // namespace

void Input::begin() {
  queue = xQueueCreate(16, sizeof(Action));
  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  pinMode(PIN_ENC_SW, INPUT_PULLUP);
  encPrev = (digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), encISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_B), encISR, CHANGE);
}

void Input::post(Act t, int32_t v) {
  Action a{t, v};
  xQueueSend(queue, &a, 0);
}

bool Input::next(Action &a) { return xQueueReceive(queue, &a, 0) == pdTRUE; }

void Input::poll() {
  // rotation: one action per detent
  int32_t ticks = encTicks;
  int32_t detents = (ticks - encUsed) / ENC_STEPS_PER_DETENT;
  if (detents) {
    encUsed += detents * ENC_STEPS_PER_DETENT;
    bool cw = (detents > 0) != (bool)ENC_REVERSE;
    for (int i = 0; i < abs(detents); i++) post(cw ? Act::TurnCW : Act::TurnCCW);
  }

  // button (active low), 25ms debounce
  bool raw = digitalRead(PIN_ENC_SW) == LOW;
  uint32_t now = millis();
  if (raw != btnLastRaw) { btnLastRaw = raw; btnChange = now; }
  if (now - btnChange > 25 && raw != btnStable) {
    btnStable = raw;
    if (raw) { btnDown = now; longFired = false; }
    else if (!longFired) post(Act::Press);
  }
  if (btnStable && !longFired && now - btnDown >= LONG_PRESS_MS) {
    longFired = true;
    post(Act::LongPress);
  }
}
