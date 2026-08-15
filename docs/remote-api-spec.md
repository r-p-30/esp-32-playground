# Desk Mate — Remote Control API Spec

The device is USB-powered (no battery to conserve), so instead of
periodically polling, it holds one permanent WebSocket connection to the
hosted site for its entire runtime while `RemoteApi.h` has real values in
it. It's entirely optional — the device works fully offline if this is
never set up.

## Endpoint

`wss://<host>/ws/device` (`REMOTE_API_URL` in `RemoteApi.h`, gitignored —
see `RemoteApi.example.h` for the template)

The device connects once (right after WiFi comes up, see `DeskMate.ino`'s
`setup()`) and sends this as an extra header on the WebSocket handshake:

```http
X-Device-Key: <REMOTE_API_KEY>
```

Check this server-side and reject anything that doesn't match, so a
stranger who finds the URL can't remotely buzz the device or change what
it shows. If the connection ever drops (network blip, server restart),
the device automatically reconnects (`setReconnectInterval` in
`RemoteControl.cpp`) - no action needed on the device side.

Your server should push a text frame with the JSON below down this socket
immediately on connect, and again any time something changes (a dashboard
action, a card edit) - see "push, not poll" below.

## Message (JSON, server → device)

```json
{
  "revision": 1734000000,
  "showCard": 17,
  "cardTextIndex": 17,
  "cardText": "miss you :heart: call me tonight",
  "buzz": true,
  "triggerAnimation": true,
  "identifyPing": false,
  "cardTextAlignH": "center",
  "cardTextAlignV": "middle",
  "cardCornerEmoji": [":heart:", "", "", ""],
  "cardAnimationDurationIndex": 1,
  "cardAnimationDurationMs": 2200,
  "carouselEnabled": false,
  "carouselIntervalSec": 8,
  "nightModeEnabled": false,
  "gameModeEnabled": false
}
```

Fields split into two groups that behave differently:

### One-shot actions (only fire when `revision` is new)

- **`revision`** (number, required) — changes any time you want the device
  to react. Easiest option: use the Unix timestamp of your last edit —
  always increasing, no counter to maintain server-side. The device
  remembers the last revision it acted on and only reacts when this value
  is new, so a static/unchanged response is a safe no-op default (it just
  means "nothing changed since last time").
- **`showCard`** (number, optional) — if present, the device jumps to that
  card index once and plays the normal card-change beep (same sound as
  turning the knob). You can freely turn the knob again right
  after — this is a one-time nudge, not a lock.
- **`cardTextIndex`** + **`cardText`** (number + string, optional, sent as
  a pair) — overwrites the text of the card at that index. There are 22
  cards total (index 0-21). **Index 20 is the reserved empty slot**
  (writing to it is how you "add" a new card without reflashing) — every
  other index, 0-19 and 21, is a confirmed content card you can edit.
  Card 0 is "right now" (clock), shown at boot only if the device
  actually synced real time this boot; otherwise boot falls back to card 1
  ("good morning") - see `DeskMate.ino`'s `setup()`. Exiting night mode
  always lands on card 1 regardless of sync status. Card 5 (music
  equalizer) and card 0 (clock) have no press-animation since both
  redraw continuously on their own. Card text supports `\n` for line
  breaks, and a small set of emoji shortcodes (see below).

  Card order is just array order in `Cards.cpp` and can change if it gets
  reordered there — check that file directly if you need the current
  index for a specific card rather than relying on this doc staying
  perfectly in sync.
- **`cardTextAlignH`** (`"left"` / `"center"` / `"right"`, optional) and
  **`cardTextAlignV`** (`"top"` / `"middle"` / `"bottom"`, optional) — set
  together, tied to `cardTextIndex` like `cardText` above. **Only visible
  on cards using the generic bounce layout** (the reserved/custom slot by
  default) — every built-in animated card has its own fixed hand-drawn
  layout that ignores these. Send both together even if only one changed,
  so a partial update can't leave the other axis stale (see
  `RemoteControl.cpp`).
- **`cardCornerEmoji`** (array of 4 shortcode strings, optional, tied to
  `cardTextIndex`) — small decorative glyphs pinned to each corner,
  independent of the main text: `[topLeft, topRight, bottomLeft,
  bottomRight]`. Use `""` for no decoration in that corner. Same
  shortcode list as the emoji table below; same bounce-layout-only
  caveat as alignment above.
- **`cardAnimationDurationIndex`** + **`cardAnimationDurationMs`** (number
  + number, optional, sent as a pair) — overwrites how long that card's
  short-press animation runs, in milliseconds. Meant for tuning/testing
  timings live without reflashing; has no effect on the clock or
  equalizer cards since they don't have a press-animation. 1000ms is the
  default for most cards; Pac-Man defaults to 2200ms since it's tracing a
  full maze path.
- **`buzz`** (boolean, optional) — if true, plays a distinct 3-beep alert
  pattern once.
- **`triggerAnimation`** (boolean, optional) — if true, plays whatever
  card is currently showing its short-press animation, without anyone
  touching the button. If combined with `showCard` in the same update,
  it plays on the card just jumped to. No-op while in night mode or game
  mode (there's no card animation to play).
- **`identifyPing`** (boolean, optional) — if true, the device beeps
  twice and briefly inverts the screen, then puts everything back exactly
  as it was. Doesn't touch card content, doesn't change the active card —
  purely a "did my update actually reach the device" check, useful while
  building/debugging the site so a blank result doesn't leave you
  guessing whether it's a site bug or a connectivity problem.

Any subset of the above can be sent together in one update — e.g. bump
`revision` with only `buzz: true` and nothing else to just make it buzz,
or only `cardTextIndex`/`cardText` to silently edit a card's text without
jumping to it or buzzing.

### Continuous settings (applied from every pushed message, not tied to `revision`)

- **`carouselEnabled`** (boolean, default false) — when true, the device
  auto-advances to the next card on its own, no knob input needed.
  **Safety cap: auto-stops after 1 hour of continuous running, enforced
  by the device itself regardless of what this field says.** To run it
  again, explicitly send `false` then `true` again (a real stop/restart)
  — leaving it `true` forever in your saved state will not keep it going
  past the hour. This exists so a forgotten "on" toggle can't quietly
  drain the battery.
- **`carouselIntervalSec`** (number, default 5) — how often it advances.
  The timer resets on *any* card change (manual, remote jump, or
  carousel), so it always measures "time since the last change."
- **`nightModeEnabled`** (boolean, default false) — remote equivalent of a
  5-second press-and-hold on the knob (see button reference below):
  dims the screen and shows a full-screen inverted clock instead of
  cards.
- **`gameModeEnabled`** (boolean, default false) — remote equivalent of a
  2-second press-and-hold: switches to the game-mode placeholder screen
  (the actual game isn't built yet). Ignored while night mode is active.

`carouselEnabled` behaves exactly as described above — the device just
keeps applying whatever value came in the last push, so leaving it
absent/false is a safe default.

**`nightModeEnabled` and `gameModeEnabled` behave differently, because
they can also be toggled by the physical button:** the device only reacts
when *your saved value itself changes* between pushes, not to the value
being persistently true. Concretely — if you set `nightModeEnabled: true`
once, the device turns night mode on. If someone then long-presses
to turn it back off locally, it **stays off** even though your database
still has `nightModeEnabled: true` sitting in it, because nothing about
your stored value changed on the next push. To turn it on again
remotely, you have to actually write a new value (e.g. `false` then
`true`, or just re-save with a slightly different flow) so the device
sees a genuine transition. Plan your site's UI around this — a toggle
switch that always reflects "the last thing I sent," not "what's
currently active on the device" (the heartbeat below is how you'd show
the latter).

### Emoji shortcodes

The OLED's built-in font can't render real Unicode emoji at all (multi-byte
UTF-8 just shows up as garbled box-drawing characters) — but it does
include a handful of old IBM CP437 "symbol" glyphs as single characters.
Use these shortcodes in `cardText` and the device swaps them in automatically;
build your site's emoji picker around this exact list rather than letting
people type arbitrary emoji:

| Shortcode | Renders as |
|---|---|
| `:heart:` | ♥ |
| `:smile:` | ☺ |
| `:smileb:` | ☻ (filled) |
| `:spade:` | ♠ |
| `:club:` | ♣ |
| `:diamond:` | ♦ |
| `:note:` | ♪ |
| `:notes:` | ♫ |

Worth a visual check on real hardware once flashed — these are standard
CP437 codepoints but I haven't been able to render-test them myself.

## Physical button reference

The site's controls are meant to mirror what the physical knob can already
do, so the device can be driven the same ways either at the device itself
or remotely. Short/long press behavior depends on what's currently showing
(see `loop()` in `DeskMate.ino`):

| Input | While in... | Effect | Remote equivalent |
|---|---|---|---|
| Rotate | Cards | Next/previous card | `showCard` |
| Short press | Cards | Play current card's animation | `triggerAnimation` |
| Short press | Night mode | Exit to cards | `nightModeEnabled: false` |
| Short press | Game mode | Reserved for gameplay (not built yet) | — |
| Long press (≥2s) | Cards | Enter game-mode placeholder | `gameModeEnabled: true` |
| Long press (≥2s) | Game mode | Exit to cards | `gameModeEnabled: false` |
| Long press (≥2s) | Night mode | Exit to cards | `nightModeEnabled: false` |
| Very long press (≥5s) | Cards | Enter night mode | `nightModeEnabled: true` |
| Very long press (≥5s) | Night mode | Exit to cards | `nightModeEnabled: false` |
| Very long press (≥5s) | Game mode | Exit to cards (does **not** enter night mode) | `gameModeEnabled: false` |

Night mode is the one screen where every press length does the same thing
(exit to cards) - no need to guess the right press duration just to get
out of it.

## Heartbeat (device → site, optional)

The main state message above flows one direction (site → device). The
device also sends a small heartbeat *back* over the same open socket
every `REMOTE_HEARTBEAT_INTERVAL_MS` (default 30s, `Config.h`) as a text
frame:

```json
{"currentCard":0,"nightMode":false,"gameMode":false,"uptimeSec":184320}
```

- `currentCard` — index of whatever's currently showing.
- `nightMode` / `gameMode` — current mode flags, so your UI can show
  what's actually active on the device right now (as opposed to
  `nightModeEnabled`/`gameModeEnabled` above, which only reflect the last
  thing *you* sent, not the current truth — see the note on those fields).
- `uptimeSec` — seconds since last boot (resets to a small number if the
  device loses power/resets).

The device doesn't expect a reply - this is fire-and-forget, so your
server just needs to parse any inbound WebSocket message as this shape
and save it with a timestamp. Since it's the same permanent connection
the state pushes go out on, a live connection is itself already a
stronger "is it online" signal than the heartbeat content - the site's
dashboard shows both (see `device_ws` in `app.py`).

This is what makes "last seen 2 min ago" (and "connected right now")
possible in the UI, instead of guessing whether the device is reachable.

## Security notes

- `X-Device-Key` is a shared secret, not real authentication — enough to
  stop casual/accidental hits on the URL, not a targeted attacker.
- The device does not validate your server's TLS certificate (empty
  fingerprint passed to `beginSSL()` in `RemoteControl.cpp`) — traffic is
  encrypted in transit but not verified against a specific identity, so
  it's not resistant to a targeted machine-in-the-middle attack. Fine for
  a personal project; don't put anything sensitive through this channel.

## Implementation note: push, not poll

This used to be a plain `GET` the device polled on an interval. Now that
the device is USB-powered, it holds one permanent WebSocket connection
instead (`RemoteControl.cpp`), and the server pushes a new state message
the instant something changes - no polling delay, and no fixed interval
to tune for responsiveness vs. battery life. The site (`site/app.py`)
keeps a single open connection (`_device_ws`) and calls
`_push_state_to_device()` right after every dashboard/card-manager action
that calls `state.apply_update(...)`. Only one device is expected to be
connected at a time, matching this being a single-recipient personal
project - the server doesn't try to fan a push out to multiple
connections.

A minimal test server just needs to accept a WebSocket connection at
`/ws/device`, check the `X-Device-Key` header on the handshake, and send
the JSON above as a text frame. `flask-sock` (already used in `site/`)
makes this a small addition to a normal Flask app rather than a separate
service.
