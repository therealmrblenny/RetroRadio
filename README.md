# RetroRadio

A wall-plug internet radio for the hi-fi: a Sonocotta HiFi-ESP32 board, a 32×8 blue
LED marquee, one knob, and a 3D-printed black case. Line out to an amp.

Write-up, photos and videos: **https://retroradio.ventureindustries.au**

## What's here

| Folder | What |
|---|---|
| `firmware/` | PlatformIO project (Arduino on ESP32). |
| `firmware/sim/` | PC simulator for the display: runs the real view code and plays it back in a browser (`preview.html`). |
| `case/` | OpenSCAD source (`retroradio.scad`) and print-ready STLs in `case/stl/`. |
| `shopping-list.html` | Bill of materials (Jaycar part numbers). |
| `wiring-diagram.html` | Signal and power wiring, plus a pin-by-pin checklist. |

## Firmware in brief

- **Audio:** [ESP32-audioI2S](https://github.com/schreibfaul1/ESP32-audioI2S) streams
  MP3/AAC/HLS to the board's DAC. TCP window raised to 32 KB so distant high-bitrate
  streams don't stutter.
- **Display:** two AIP1640 LED modules bit-banged through level shifters, drawn as one
  33-column canvas (the hidden gap column keeps scrolling smooth across the join).
  Custom 5×7 and compact capitals fonts.
- **Knob:** turn for volume; press, turn, press to change station; hold for standby.
- **Standby:** little pixel-art scenes (a cat chasing a moth, a frog catching flies, a
  sunrise) instead of a blank screen.
- **Web UI** at `http://retroradio.local`: controls, a virtual knob, station lists, and
  search across the radio-browser.info directory (the search runs in your browser,
  not on the radio).
- **Setup:** if it can't join WiFi it starts a `RetroRadio-Setup` hotspot; join it,
  open 192.168.4.1, pick your network.
- **Updates** over the air with ArduinoOTA.

## Building

```
cd firmware
cp secrets.ini.example secrets.ini            # OTA password
cp include/secrets.h.example include/secrets.h  # default WiFi + OTA password
pio run -e usb -t upload                     # first flash over USB
```

Pins and display options (flip, module swap, encoder direction) are in
`firmware/include/config.h`. Check the wiring sheet before powering up: the matrices
and level shifters' HV side run on 5 V from the USB-C PD breakout, the LV side on
the board's 3.3 V, and every ground must be joined.

## Printing the case

Matte black PLA for `body`, `base`, `panel` and `grid`; frosted transparent PETG for
`window` (`window_test` is a small test piece). M3 heat-set inserts and M3 screws.
Optional: window tint film over the PETG for a black-when-off display.

## Licence

- **Firmware and tools** (`firmware/`): MIT. See `LICENSE`.
- **Case design, shopping list and wiring sheet** (`case/`, `shopping-list.html`,
  `wiring-diagram.html`): Creative Commons Attribution 4.0 (CC BY 4.0). See
  `case/LICENSE`. Print it, remix it, sell it. Just credit "RetroRadio by Damian
  Macrae" and link back.
- The firmware pulls in [ESP32-audioI2S](https://github.com/schreibfaul1/ESP32-audioI2S)
  at build time, which is GPL-3.0, so a compiled firmware binary you hand to someone
  else falls under GPL-3.0 terms (offer the source; this repo is it).
