# CLAUDE.md — Desk Mate (ESP32 device + companion site)

## Context

This is one project built in a single phase — firmware and companion
site together, not a phased rollout. It's a personal/hobby build for
a small number of users, not a product. Prioritize simplicity and
readability over scalability, abstraction, or defensive engineering.

**What it is:** an ESP32 + OLED + rotary encoder + buzzer desk device
that cycles through personal message "cards," each with its own
animation and sound feedback, fully offline-capable on its own. A
companion Flask site (`site/`) optionally drives it remotely over a
permanent WebSocket connection — editing card content, buzzing it,
triggering animations, toggling night/game mode, running a carousel —
without reflashing.

Full details live in [README.md](README.md) (kept up to date — treat
it as the source of truth alongside the actual code, not this file) and:

- `docs/desk-mate-project-plan.md` — parent/overview plan; also covers WiFi, remote control, deployment, and the PWA setup
- `docs/card-mode-plan.md` — hardware, wiring, parts list, cards, night mode
- `docs/firmware-setup.md` — Arduino IDE/arduino-cli toolchain setup for a new machine
- `docs/remote-api-spec.md` — the exact JSON/WebSocket contract between site and device
- `docs/site-project-plan.md` — site design notes
- `docs/game-mode-plan.md` — specs for the game-mode games

Note: the docs above are planning documents and may describe things
that changed once actually implemented — the firmware
(`firmware/DeskMate/`) and site (`site/`) code is the real source of
truth when they disagree.

---

## What's actually built

**Firmware** (`firmware/DeskMate/`, Arduino/C++):

- 24 total card slots: **23 pre-written content cards plus 1 reserved
  custom slot** (index 22) that starts empty and can be filled in
  remotely from the site without reflashing.
- Three button tiers on the rotary encoder (short/long/very-long press,
  500ms/2000ms thresholds in `Config.h`) for animation playback, the
  game-mode picker menu, and night mode.
- Fully offline-first: WiFi/NTP is optional, only used for time sync
  and (if configured) the remote link.
- Optional remote control over one permanent WebSocket
  (`RemoteControl.cpp`) — no polling. Covers jumping cards, editing
  card text/alignment/corner emoji, emoji-shortcode animations, buzz/
  identify/trigger-animation, carousel, night mode, game mode (including
  launching a specific game remotely).
- 5 real games implemented (`Games.cpp` + each game's own `*Engine.cpp`):
  Dino Jump, Whack-a-mole, Snake, Tetris, Car Racing — see
  `docs/game-mode-plan.md`. One picker slot is still a reserved
  stub ("Tank Battle (soon)"); Tank Battle and a deferred Tank Arena are
  specced but not implemented yet.

**Companion site** (`site/`, Flask + `flask-sock`):

- Plain Jinja2 templates + vanilla CSS/JS (`static/`) — no frontend
  framework or build step.
- Single dashboard page (`/dashboard`): status/quick-actions, card
  editor, and a live 128×64 canvas preview side by side — not a
  separate page per card.
- Login-gated (`SITE_PASSWORD`) for the human UI; separate shared-secret
  (`X-Device-Key` / `DEVICE_API_KEY`) for the device's WebSocket.
- State persisted as plain JSON files under `site/instance/` (see
  `state.py`) — deliberately not a database, resets on Render's free-tier
  idle restarts (the device's `cardsReport` on reconnect self-heals it).
- Deployed and live on Render (`render.yaml`).

**Not built / out of scope:** image upload or per-card image
composition, a PWA service worker (there's a `manifest.json` for
installability, but no offline shell caching), user accounts beyond
the single shared password, Tank Battle/Tank Arena gameplay.

---

## Conventions to keep following

- **Site frontend stays vanilla**: plain HTML/CSS/JS via Flask/Jinja2,
  no framework, no build step, no npm/bundler.
- **Site backend stays minimal Flask**: `flask-sock` for the device
  WebSocket, JSON-file persistence, no ORM, no microservices, no auth
  beyond the two shared secrets already in place.
- **Mobile-first CSS** — the dashboard is used as an installed
  phone-home-screen app first, desktop is the secondary layout.
- Prefer clear, conventional code and comments that explain *why*
  (a mirrored constant, a non-obvious ordering, a workaround) over
  restating *what* the code does.
- The OLED is monochrome 128×64 — any preview UI must honestly show
  what will actually render on the real device, not a generic web
  preview.
