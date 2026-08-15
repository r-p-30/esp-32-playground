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

Wiring and pin assignments are in `docs/desk-mate-project-plan.md` and mirrored
in `firmware/DeskMate/Config.h`.

## What it does

**21 content cards** (plus 1 reserved empty slot), each with its own
short-press animation.

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

If `firmware/DeskMate/RemoteApi.h` is filled in with a real `wss://`
endpoint, the device holds one **permanent WebSocket connection** to the
companion site in `site/` for its entire runtime (`RemoteControl.cpp`) —
no polling. The device is USB-powered, so there's no battery cost to
staying connected, and the site pushes a fresh state message down the
socket the instant something changes (a dashboard click, a card save) —
not on a fixed interval. If the connection drops (network blip, server
restart), the device auto-reconnects on its own.

**Socket details** (full contract in `docs/remote-api-spec.md`):

- Endpoint: `wss://<host>/ws/device`, `REMOTE_API_URL` in `RemoteApi.h`
- Auth: `X-Device-Key: <REMOTE_API_KEY>` header on the WebSocket handshake — shared secret, checked server-side, rejects anything else
- Server → device: a JSON state message, pushed on connect and on every change
- Device → server: a small heartbeat (current card, mode, uptime) sent back over the same socket every `REMOTE_HEARTBEAT_INTERVAL_MS` (default 30s) — this is what lets the dashboard show "last seen" and "● Connected" independent of the state pushes
- The device does not validate the server's TLS certificate (fine for a personal project — see the security notes in `docs/remote-api-spec.md`)

Once connected, it can be remotely told to:

- Jump to any card, with the normal navigation beep
- Edit any existing card's text, or fill the reserved slot to add a new card without reflashing
- Set that card's text alignment (left/center/right, top/middle/bottom) and pin up to 4 corner-emoji decorations — only visible on cards using the generic layout (the reserved slot by default), since every built-in animated card has its own fixed hand-drawn layout
- Insert a small set of emoji-like symbols via shortcodes (`:heart:`, `:smile:`, `:note:`, etc. — the OLED's font can't render real Unicode emoji)
- Buzz the device, or trigger the active card's animation, without anyone touching the button
- Send an "identify" ping (double-beep + screen flash) to confirm connectivity while debugging the site
- Run a carousel (auto-advancing cards) with a configurable interval — hard-capped at 1 hour of continuous runtime so a forgotten toggle can't leave it running indefinitely
- Toggle night mode or game mode remotely (mirrors the physical long/very-long press)

All of this is off by default and entirely optional — the device works
fully offline if `RemoteApi.h` is never configured.

The site also has a login-gated dashboard and card manager for driving all
of the above by hand. Full field-by-field contract: `docs/remote-api-spec.md`.
Site design doc: `docs/site-project-plan.md`.

## Companion site (`site/`)

A small Flask app (mobile-first PWA), deployed and running on Render, with two halves:

- **Device-facing** (`/api/state` for manual/debug reads, `/ws/device` for the real permanent connection) — shared-secret auth via `X-Device-Key`, matched against `REMOTE_API_KEY` in the device's `RemoteApi.h`. Served via `gunicorn --worker-class gthread` so the held-open device socket can't block regular page requests.
- **Human-facing UI** (`/dashboard`, `/cards`) — password-gated login (separate secret from the device key, deliberately, so leaking one doesn't compromise the other).

**Dashboard**: buzz / play animation / identify quick actions, carousel config, night/game mode send buttons, and a live status line — "● Connected"/"● Not connected" plus "last seen Ns ago" from the heartbeat. Every action shows a flash-message confirmation, so it's never ambiguous whether something saved.

**Cards** (`/cards?edit=<index>`): a single-page builder, not a stacked list of 22 forms. A compact numbered picker selects which card to edit; the form has the text box + 8-shortcode emoji-insert row, alignment controls, 4 corner-emoji pickers, and animation duration, with a **live canvas preview** (128×64, scaled up) that updates as you type — catches overflow/alignment mistakes before they reach the real device. Saving is one combined action (text + alignment + corner emoji + duration, one revision bump), separate from "make active." Desktop shows fields and preview side-by-side sized to fit without page scroll; mobile stacks fields → emoji pickers → preview.

State is persisted via `state.py`; see `site/state.py` for the storage details.

### Site setup

1. `cd site && python -m venv venv && venv\Scripts\activate` (PowerShell), then `pip install -r requirements.txt`.
2. Copy `.env.example` → `.env` and fill in `FLASK_SECRET_KEY`, `SITE_PASSWORD`, and `DEVICE_API_KEY` (the last one must match `REMOTE_API_KEY` in the firmware's `RemoteApi.h` exactly). `.env` is gitignored, never commit it.
3. `python app.py` for local dev (runs on `:5000`), or deploy with `gunicorn --worker-class gthread --threads 4 app:app` — `render.yaml` documents the same command. **Deployed and live on Render** (free tier — spins down after ~15 min idle, first request after that wakes it back up).

## Setup (firmware)

1. Install the ESP32 board core + Adafruit SSD1306, Adafruit GFX, ArduinoJson, WiFiManager, and WebSockets (Markus Sattler / `Links2004/arduinoWebSockets`) libraries (Arduino IDE Library Manager, or `arduino-cli lib install`).
2. **Board setting**: Tools → Partition Scheme → **"Huge APP (3MB No OTA/1MB SPIFFS)"**. The WebSockets library pushes flash usage past the default scheme's limit — this isn't optional. Changing it reshuffles flash layout, so the first flash after switching may force a fresh WiFiManager setup even on a previously-configured board.
3. Copy `WifiCredentials.example.h` → `WifiCredentials.h` and fill in a name/password for the temporary WiFiManager setup portal (gitignored, never commit). Actual WiFi credentials aren't hardcoded anywhere — the device reconnects via whatever's already stored in NVS from a past successful connect, and falls back to that setup portal if nothing's stored yet.
4. Optional — copy `RemoteApi.example.h` → `RemoteApi.h` and fill in the site's `wss://` endpoint + the shared `DEVICE_API_KEY` to enable remote control (also gitignored).
5. Flash `firmware/DeskMate/DeskMate.ino`.

Compile check without the IDE:

```powershell
arduino-cli compile --fqbn "esp32:esp32:esp32:PartitionScheme=huge_app" firmware/DeskMate
```

## Status

Firmware compiles clean against `esp32:esp32@3.3.11` (~41% flash, ~16%
RAM, with the Huge APP partition scheme). Bench-tested on real hardware
for the core loop (OLED, encoder, buzzer, WiFi/NTP, WiFiManager
captive-portal setup). Remote control (permanent WebSocket, card
alignment/corner-emoji, night mode, game-mode placeholder) is built and
compiling. The companion site is built, tested locally (login, dashboard
actions, WebSocket push/heartbeat round-trip all verified against a
simulated device), and **deployed live on Render** — not yet run
end-to-end against the real physical device.

## Roadmap

- Real game mode (currently just a placeholder screen behind the long-press toggle)
- End-to-end test of the deployed site against real hardware (reflash with the current partition scheme + `RemoteApi.h` pointed at the live Render URL)
