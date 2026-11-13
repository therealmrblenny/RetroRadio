#include "matrix.h"
#include "config.h"

namespace {
uint8_t brightness = 3;
bool displayOn = true;
uint8_t last[MTX_W];
bool dirty = true;
uint32_t lastPush = 0;

inline void dly() { delayMicroseconds(MTX_BIT_US); }

// AIP1640 / TM1640 two-wire protocol: start = DIN falls while CLK high,
// data bits LSB-first latched on CLK rising edge, stop = DIN rises while CLK high.
void startCond(int din) {
  digitalWrite(din, HIGH); digitalWrite(PIN_MTX_CLK, HIGH); dly();
  digitalWrite(din, LOW); dly();
  digitalWrite(PIN_MTX_CLK, LOW); dly();
}
void stopCond(int din) {
  digitalWrite(PIN_MTX_CLK, LOW); digitalWrite(din, LOW); dly();
  digitalWrite(PIN_MTX_CLK, HIGH); dly();
  digitalWrite(din, HIGH); dly();
}
void writeByte(int din, uint8_t b) {
  for (int i = 0; i < 8; i++) {
    digitalWrite(PIN_MTX_CLK, LOW);
    digitalWrite(din, (b >> i) & 1); dly();
    digitalWrite(PIN_MTX_CLK, HIGH); dly();
  }
  digitalWrite(PIN_MTX_CLK, LOW);
}
void sendModule(int din, const uint8_t *cols16) {
  startCond(din); writeByte(din, 0x40); stopCond(din);           // data cmd: auto-increment
  startCond(din); writeByte(din, 0xC0);                          // address 0
  for (int i = 0; i < 16; i++) writeByte(din, cols16[i]);
  stopCond(din);
  startCond(din); writeByte(din, displayOn ? (0x88 | brightness) : 0x80); stopCond(din);
}
uint8_t rev8(uint8_t b) {
  b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
  b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
  return (b & 0xAA) >> 1 | (b & 0x55) << 1;
}
}  // namespace

void Matrix::begin() {
  pinMode(PIN_MTX_CLK, OUTPUT);  digitalWrite(PIN_MTX_CLK, HIGH);
  pinMode(PIN_MTX_DIN1, OUTPUT); digitalWrite(PIN_MTX_DIN1, HIGH);
  pinMode(PIN_MTX_DIN2, OUTPUT); digitalWrite(PIN_MTX_DIN2, HIGH);
  memset(last, 0, sizeof(last));
  dirty = true;
}

void Matrix::setBrightness(uint8_t level) {
  level = min<uint8_t>(level, 7);
  if (level != brightness) { brightness = level; dirty = true; }
}
void Matrix::setOn(bool on) { if (on != displayOn) { displayOn = on; dirty = true; } }

void Matrix::show(const uint8_t *cols) {
  // Only push on change, plus a 1s refresh in case a module glitched/was hot-plugged
  if (!dirty && memcmp(cols, last, MTX_W) == 0 && millis() - lastPush < 1000) return;
  memcpy(last, cols, MTX_W);
  dirty = false; lastPush = millis();

  uint8_t phys[MTX_W];
  for (int x = 0; x < MTX_W; x++) {
    uint8_t c = cols[MTX_FLIP_X ? (MTX_W - 1 - x) : x];
    phys[x] = MTX_FLIP_Y ? rev8(c) : c;
  }
  const uint8_t *left = phys, *right = phys + 16;
  if (MTX_SWAP_MODULES) std::swap(left, right);
  sendModule(PIN_MTX_DIN1, left);
  sendModule(PIN_MTX_DIN2, right);
}
