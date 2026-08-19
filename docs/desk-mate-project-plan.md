# ESP32 Desk Mate — Project Plan (Overview)

**Concept:** a desk device built on an ESP32 — turn a knob to browse personal message "cards" on a small OLED, each with its own animation and sound feedback, fully offline-capable on its own, with a hidden game mode and optional full remote control from a companion Flask site.

This is the parent/overview plan. It intentionally stays high-level for the card/game split, but keeps the shared infrastructure that both modes depend on — WiFi, remote control, deployment, and the PWA setup — here rather than duplicating it into either child doc.

| Doc | Covers |
|---|---|
| [card-mode-plan.md](card-mode-plan.md) | Hardware build (parts list, wiring), the card content system, and night mode |
| [game-mode-plan.md](game-mode-plan.md) | The hidden game-mode games (Dino Jump, Whack-a-Mole, Snake, Tetris, Car Racing, and more planned) — deliberately skips hardware-level detail, since that's all covered in the card mode plan above |

Related docs outside this card/game split:

- [firmware-setup.md](firmware-setup.md) — Arduino IDE/arduino-cli toolchain setup for a machine that's never built this before (board support, libraries, config files)
- [remote-api-spec.md](remote-api-spec.md) — the exact JSON/WebSocket contract between the companion site and the device
- [site-project-plan.md](site-project-plan.md) — design notes for the companion site itself

`README.md` (repo root) stays the single most up-to-date source of truth alongside the actual firmware/site code — these docs describe intent and design reasoning, but if either ever disagrees with what's actually built, the code (and README) wins.

---

## WiFi & Remote Control

The core device is **fully offline-first** — none of card browsing, night mode, or game mode needs WiFi. WiFi is used for two optional things: a one-time NTP time sync at boot, and (if configured) the permanent remote-control connection.

**Boot-time connect sequence** (`connectWiFiAtBootWithSetupFallback()`, `firmware/DeskMate/WifiUtil.cpp`), tried in order, each skippable early via a long-press on the encoder:

1. **Hardcoded credentials** (`WIFI_SSID`/`WIFI_PASSWORD` in `WifiCredentials.h`, gitignored) — tried first if filled in, on a fresh radio state.
2. **Stored NVS credentials** — whatever's left over from a past successful connect (most likely a prior WiFiManager portal run). This is the path a recipient's device (no hardcoded credentials at all) actually uses.
3. **WiFiManager setup portal** — if both above fail, the device becomes a temporary WiFi AP with a captive portal (`WIFI_SETUP_AP_NAME`/`WIFI_SETUP_AP_PASSWORD`, also in `WifiCredentials.h`): connect to it from a phone, pick a real network, enter its password once. Bounded to `WIFI_SETUP_PORTAL_TIMEOUT_SEC` (240s / 4 min) so a device that's never been configured still finishes booting into the offline card browser instead of hanging.

If every tier fails (or is skipped), the device proceeds fully offline using its internal clock — this must never block or break the core card-browsing experience.

**NTP time sync** happens once at boot if WiFi connected, with an HTTPS-header fallback if NTP itself is blocked on the network (`TimeSync.cpp`). Once synced, the device tracks elapsed time locally without needing a continued connection — this is also what feeds card mode's clock card and night mode's clock display.

**Remote control** (optional, off by default): if `RemoteApi.h` is filled in with a real `wss://` endpoint and shared device key, the device holds one permanent WebSocket connection to the companion site for its entire runtime (`RemoteControl.cpp`) — no polling, auto-reconnects on drops. The device is USB-powered, so there's no battery cost to staying connected, and the site pushes a fresh state message down the socket the instant something changes rather than on a fixed interval. This is what lets the site edit cards, jump/buzz/trigger animations, run a carousel, and toggle night/game mode (including launching a specific game directly) remotely. Full JSON/WebSocket contract: [remote-api-spec.md](remote-api-spec.md). Companion site design: [site-project-plan.md](site-project-plan.md).

---

## Deployment

The companion site (`site/`) is deployed on **Render**, config'd via `site/render.yaml`:

- `gunicorn --worker-class gthread --threads 4 app:app` — not the default sync worker, deliberately: the device's permanent `/ws/device` connection stays open for its entire runtime, and a sync worker would let that one held-open connection block every other request (login, dashboard) for as long as it's connected. Threads let them coexist on one worker process.
- Three secrets are set as Render environment variables (never committed): `FLASK_SECRET_KEY`, `SITE_PASSWORD`, `DEVICE_API_KEY` — the last one must match `REMOTE_API_KEY` in the firmware's `RemoteApi.h` exactly, or the device's WebSocket handshake gets rejected.
- Render's free tier spins the service down after ~15 minutes idle; the first request after that wakes it back up (a few seconds' delay) rather than failing outright. State is JSON files under `site/instance/` (see `state.py`) rather than a database, so a spin-down restart loses in-memory state — the device's `cardsReport` on reconnect is what self-heals the site's copy of card content rather than the site needing durable storage.

Local dev: `cd site && python -m venv venv`, `pip install -r requirements.txt`, copy `.env.example` → `.env`, `python app.py` (runs on `:5000`). Full step-by-step: `README.md`'s "Site setup" section.

---

## PWA (installability)

The dashboard is meant to be used as an installed phone-home-screen app first, browser tab second — `site/static/manifest.json` declares the app name, standalone display mode, theme colors, and 192px/512px icons so mobile browsers offer an "Add to Home Screen" install prompt pointing at `/dashboard`.

**Deliberately not built:** a service worker or any offline shell caching. Installing the icon just gives a chrome-less shortcut into the live site — there's no offline mode, no cached dashboard state, no background sync. This matches the site's own nature (a live control surface for a device that's either connected or not) rather than a content app worth caching for offline reading.
