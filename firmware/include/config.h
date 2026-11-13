#pragma once
// ---- HiFi-ESP32 audio (Sonocotta docs) ----
#define PIN_I2S_BCK   26
#define PIN_I2S_DOUT  22
#define PIN_I2S_WS    25

// ---- 2x XC3746 LED matrix (AIP1640) via XC4486 level shifters, on the W5500 header ----
#define PIN_MTX_CLK   18   // header "CLK"  - shared by both modules
#define PIN_MTX_DIN1  23   // header "MOSI" - left module
#define PIN_MTX_DIN2  19   // header "MISO" - right module
#define MTX_BIT_US    8    // half-bit delay; XC4486 shifters are slow, keep >= 5
#define MTX_SWAP_MODULES 0 // set 1 if left/right halves are swapped
#define MTX_FLIP_X    0    // set 1 if text is mirrored
#define MTX_FLIP_Y    0    // set 1 if text is upside down
#define MTX_W         32
#define MTX_H         8

// ---- SR1230 rotary encoder on the back pads ----
#define PIN_ENC_A     33
#define PIN_ENC_B     27
#define PIN_ENC_SW    21
#define ENC_STEPS_PER_DETENT 2  // SR1230: 30 detents / 15 pulses = 2 transitions per click
#define ENC_REVERSE   0         // set 1 if clockwise turns the volume down

// ---- behaviour ----
#define HOSTNAME      "retroradio"
#define AP_SSID       "RetroRadio-Setup"
#define VOL_MAX       21
#define LONG_PRESS_MS 700
#define BROWSE_TIMEOUT_MS 8000
#define VOL_OVERLAY_MS    1500
