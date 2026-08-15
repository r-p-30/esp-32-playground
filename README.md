# esp-32-playground

A desk device built on an ESP32: turn a knob to browse through personal
message "cards" on a small OLED, with sound feedback. Built in phases —
offline card browser first, WiFi features and a hidden game mode later.

## Structure

- `firmware/GiftDevice/` — Phase 1 sketch (card browser, offline-first, one NTP sync at boot)
- `docs/gift-project-plan.md` — full project plan (parts list, wiring, phases, evening-by-evening schedule)

## Hardware (Phase 1)

- ESP32 NodeMCU (CP2102 USB-UART)
- 1.3" I2C OLED, 128x64 (SSD1306)
- KY-040 rotary encoder (with push-button)
- Passive piezo buzzer

Wiring and pin assignments are in `docs/gift-project-plan.md` and mirrored
in `firmware/GiftDevice/Config.h`.

## Status

Phase 1 firmware compiles clean against `esp32:esp32@3.3.11`. Not yet
tested on real hardware beyond the toolchain/Blink sanity check — OLED,
encoder, and buzzer wiring/testing still pending.

## Roadmap

See `docs/gift-project-plan.md` section 5 for the full phased plan
(Phase 2: hosted config site + remote card updates + WiFiManager;
Phase 3 stretch: hidden dino-jump game mode).
