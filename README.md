# esp-32-playground

A desk mate device built on an ESP32: turn a knob to browse personal
message "cards" on a small OLED, each with its own animation, sound
feedback, and — optionally — full remote control from a hosted site.

## Structure

- `firmware/DeskMate/` — the sketch
- `firmware/sanity-checks/` — throwaway bring-up tests for each component (blink, OLED, buzzer, encoder) — not the real firmware
- `site/` — the companion Flask site (dashboard + card manager) that remotely controls the device
- `docs/desk-mate-project-plan.md` — hardware, wiring, parts list, phased build plan
- `docs/remote-api-spec.md` — the exact JSON contract the site implements to control the device
- `docs/site-project-plan.md` — design doc for the companion site

## Hardware

- ESP32 NodeMCU (CP2102 USB-UART)
- 1.3" I2C OLED, 128×64 (SSD1306)
- KY-040 rotary encoder (with push-button)
- Passive piezo buzzer
- 3×AA battery pack (for portability, once WiFi power draw is known)

Wiring and pin assignments are in `docs/desk-mate-project-plan.md` and mirrored
in `firmware/DeskMate/Config.h`.

## What it does

**21 content cards** (plus 1 reserved empty slot), each with its own
short-press animation — Pac-Man in an actual maze, a two-flower bouquet
with a butterfly, terminal-styled dev jokes, a heartbeat pulse, sunrise
with birds, a starfield with a shooting star, a box that shakes open
with confetti, a live-ticking friendship-stats bar, a connect-the-dots
constellation, a simplified India map with a location pin, a Chrome-dino
homage complete with cacti and a ground line, and more. Card 0 ("good
morning") is what shows at boot; the clock card only appears once a real
time sync has succeeded that boot.

**Three physical button tiers** on the encoder's push-button:

| Press | Action |
|---|---|
| Short (<2s) | Play the current card's animation |
| Long (2-5s) | Toggle cards ⇄ game-mode placeholder (real gameplay not built yet) |
| Very long (≥5s) | Toggle night mode — dims the screen and shows a full-screen inverted clock with a crescent moon |

**Fully offline-first**: none of the above needs WiFi. A brief NTP sync
happens once at boot (with an HTTPS-header fallback if NTP is blocked on
the network) and the device works completely standalone if it never
connects at all.

## Remote control (optional)

If `firmware/DeskMate/RemoteApi.h` is filled in with a real endpoint,
the device also polls the companion site in `site/` (default every 60s,
one brief WiFi connect + HTTPS request + disconnect — not a continuous
connection) and can be remotely told to:

- Jump to any card, with the normal navigation beep
- Edit any existing card's text, or fill one of the 3 reserved slots to add a new card without reflashing
- Insert a small set of emoji-like symbols via shortcodes (`:heart:`, `:smile:`, `:note:`, etc. — the OLED's font can't render real Unicode emoji)
- Buzz the device, or trigger the active card's animation, without anyone touching the button
- Send an "identify" ping (double-beep + screen flash) to confirm connectivity while debugging the site
- Run a carousel (auto-advancing cards) with a configurable interval — hard-capped at 1 hour of continuous runtime so a forgotten toggle can't drain the battery
- Play a soft ambient ping at a random interval, within a configurable range
- Toggle night mode or game mode remotely (mirrors the physical long/very-long press)

The device can also POST a small heartbeat (current card, current mode,
uptime) back to the site each cycle, so a UI can show "last seen 2 min
ago" instead of guessing whether it's still connecting. All of this is
off by default and entirely optional — the device works fully offline if
`RemoteApi.h` is never configured.

The site also has a login-gated dashboard and card manager for driving all
of the above by hand. Full field-by-field contract: `docs/remote-api-spec.md`.
Site design doc: `docs/site-project-plan.md`.

## Companion site (`site/`)

A small Flask app (mobile-first PWA) with two halves:

- **Device-facing API** (`/api/state`, `/api/heartbeat`) — shared-secret auth via an `X-Device-Key` header, matched against `REMOTE_API_KEY` in the device's `RemoteApi.h`.
- **Human-facing UI** (`/dashboard`, `/cards`) — password-gated login (separate secret from the device key, deliberately, so leaking one doesn't compromise the other), quick actions (buzz, play animation, identify), carousel/random-notify config, night/game mode toggles, and a card manager for editing all 21 cards' text plus the reserved empty slot.

State is persisted via `state.py`; see `site/state.py` for the storage details.

### Site setup

1. `cd site && python -m venv venv && venv\Scripts\activate` (PowerShell), then `pip install -r requirements.txt`.
2. Copy `.env.example` → `.env` and fill in `FLASK_SECRET_KEY`, `SITE_PASSWORD`, and `DEVICE_API_KEY` (the last one must match `REMOTE_API_KEY` in the firmware's `RemoteApi.h` exactly). `.env` is gitignored, never commit it.
3. `python app.py` for local dev (runs on `:5000`), or deploy with `gunicorn app:app` — `render.yaml` is set up for a free Render web service.

## Setup (firmware)

1. Install the ESP32 board core + Adafruit SSD1306, Adafruit GFX, and ArduinoJson libraries (Arduino IDE Library Manager, or `arduino-cli lib install`).
2. Copy `WifiCredentials.example.h` → `WifiCredentials.h` and fill in real WiFi/hotspot credentials (gitignored, never commit).
3. Optional — copy `RemoteApi.example.h` → `RemoteApi.h` and fill in the site's endpoint + the shared `DEVICE_API_KEY` to enable remote control (also gitignored).
4. Flash `firmware/DeskMate/DeskMate.ino`.

Compile check without the IDE:

```powershell
arduino-cli compile --fqbn esp32:esp32:esp32 firmware/DeskMate
```

## Status

Firmware compiles clean against `esp32:esp32@3.3.11` (~83% flash, ~16%
RAM). Bench-tested on real hardware for the core loop (OLED, encoder,
buzzer, WiFi/NTP). Remote control, night mode, and game-mode placeholder
are built and compiling. The companion site (dashboard, card manager,
heartbeat) is built but not yet deployed/tested end-to-end against real
hardware, and battery runtime hasn't been measured yet.

## Roadmap

- Real game mode (currently just a placeholder screen behind the long-press toggle)
- Deploy the companion site and do an end-to-end test against real hardware
- Battery runtime testing once the 3×AA pack is wired to VIN
- WiFiManager swap so WiFi credentials can be entered via a captive portal, instead of hardcoded ones
