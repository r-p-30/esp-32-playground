# ESP32 Desk Mate — Card Mode Plan (Hardware, Cards, Night Mode)

Referenced by [desk-mate-project-plan.md](desk-mate-project-plan.md) (the parent overview). This doc covers the physical build and the card-browsing side of the firmware — hardware/wiring, the card content system, and night mode. WiFi/remote connectivity, deployment, and the PWA setup now live in the parent plan, since they're shared infrastructure rather than card-mode-specific. The hidden game-mode games are specced separately in [game-mode-plan.md](game-mode-plan.md), which deliberately skips hardware-level detail since it's all covered here.

---

## 1. Hardware — Parts List (confirmed, all in hand)

| Component | Product | Notes |
|---|---|---|
| Microcontroller | Robocraze ESP32 NodeMCU 30 Pin, WiFi/Bluetooth | CP2102 USB-UART chip, Micro USB |
| Display | Robocraze 1.3" I2C OLED, 128×64 (SSD1306) | I2C address **0x3C** |
| Input | Robodo KY-040 Rotary Encoder | Integrated push-button |
| Sound | 5V Passive Piezo Buzzer | Bare wire legs (normal) |
| Wiring | Female-to-female + male-to-female jumper wires, breadboard | Breadboard used as shared power/GND hub — ESP32 only has 1× 3V3 and 2× GND, not enough for every component individually |
| Power (portable) | 3×AA 4.5V battery holder, with switch | Solders to VIN/GND; USB power works fine without it |

## 2. Wiring Table

**Power/GND (via breadboard rails, male-to-female wires):**

| Component | Pin | Connects to |
|---|---|---|
| ESP32 | 3V3 | Breadboard + rail |
| ESP32 | GND | Breadboard – rail |
| OLED | VCC | Breadboard + rail |
| OLED | GND | Breadboard – rail |
| Encoder | + | Breadboard + rail |
| Encoder | GND | Breadboard – rail |
| Buzzer | – | Breadboard – rail |

**Signal pins (direct to ESP32, female-to-female wires):** — mirrored in `firmware/DeskMate/Config.h`, that file is the source of truth if this table ever drifts.

| Component | Pin | ESP32 Pin |
|---|---|---|
| OLED | SCL | GPIO22 |
| OLED | SDA | GPIO21 |
| Buzzer | + | GPIO25 |
| Encoder | CLK | GPIO18 |
| Encoder | DT | GPIO19 |
| Encoder | SW | GPIO23 |

Battery holder (once/if wired): red lead → VIN, black lead → GND (can share a GND joint with an existing wire) — no firmware changes needed, VIN accepts the ~4.5V from 3 AA batteries the same as USB's 5V.

Avoid GPIO 0, 2, 15 (boot-mode pins) and GPIO 6–11 (internal flash) for any custom wiring.

**Core concepts, for reference:** ESP32 logic runs at 3.3V (not 5V like an Arduino Uno) — every component's GND must share a common ground. I2C (OLED) uses 2 signal wires (SCL/SDA) plus power/GND; multiple I2C devices can share the same 2 wires via unique addresses. The rotary encoder's CLK/DT pins trigger in a specific order depending on spin direction — decoded here via a quadrature state table (`InputEncoder.cpp`), not simple edge detection, since a naive single-edge check produces false direction flips from mechanical bounce. SW is a separate simple push-button on the same unit, timed on release to distinguish short/long/very-long presses.

---

## 3. Cards

**23 total slots**: 22 pre-written content cards, plus 1 reserved custom slot (index 21) that starts empty and can be filled in remotely from the companion site without reflashing (`Cards.h`/`Cards.cpp`).

**Turning the knob** cycles forward/backward through cards, wrapping at both ends, with a short beep on every change. Browsing automatically skips over any still-empty reserved slot, and skips the clock card specifically if no time sync has ever succeeded this boot (`nextVisibleIndex()` — no point landing on a stuck "--:--:--").

**Pressing the knob** (thresholds from `Config.h`):

| Press | Threshold | Action |
|---|---|---|
| Short | < 500ms | Plays the current card's animation |
| Long | 500ms – 2000ms | Enters the game-mode picker menu (see [game-mode-plan.md](game-mode-plan.md)) |
| Very long | ≥ 2000ms | Toggles night mode |

**Per-card content** (`Card` struct in `Cards.h`): up to 160 characters of text, an optional bitmap icon, an animation (one of many named `ANIM_*` patterns — pacman, flower, terminal-typing variants, heart, sunrise, dino-run, clock, etc.), and a remotely-tunable animation duration. Three cards (clock, equalizer, falling-icon screensaver) redraw continuously on their own and ignore short-press entirely rather than playing a one-shot animation.

**Layout customization** (alignment, corner emoji) only applies to the generic `ANIM_BOUNCE` layout — the reserved custom slot's default — since every built-in card has its own bespoke hand-drawn layout that doesn't read those fields:
- Text alignment: horizontal (left/center/right) and vertical (top/middle/bottom)
- Up to 4 corner-emoji decorations (CP437 glyphs, one per corner), mapped from shortcodes (`:heart:`, `:smile:`, `:note:`, etc. — the OLED font can't render real Unicode emoji) since the display can't render real Unicode emoji directly
- Each emoji belongs to an animation "family" (heart-beat, smile wink/grin, sparkle, diamond, notes spin, star) that can be toggled off per-card

**Remote editing** (via the companion site, see [remote-api-spec.md](remote-api-spec.md)) can edit any card's text, fill the reserved slot to add a new card, and set alignment/corner-emoji/animation-duration — all without reflashing. A reboot always restores the 22 built-in cards' hardcoded defaults; only the reserved slot's content and any remote edits live outside flash (in RAM, self-healed from the device's `cardsReport` if the site's own state resets).

---

## 4. Night Mode

Toggled by a very-long press (≥2000ms) from the card browser, or remotely from the companion site (mirrors the physical press). While active:

- The OLED is dimmed (`display.dim(true)`) and rotation input is suppressed entirely — it's a dedicated clock, not another browsable screen.
- Shows a full-screen inverted clock: a large centered `HH:MM` in black text on a white background, plus a small crescent moon (two overlapping filled circles) drawn beside the time.
- Redraws only when the displayed minute actually changes, not every frame — avoids needless flicker/wear for a screen that's otherwise static for a full minute at a time.
- **Any press** (short, long, or very long) exits back to the card browser — no need to remember a specific press length just to get out. Exiting always lands on card 1 ("good morning") rather than wherever browsing left off, and turns the screen dimming back off.

Night mode has no WiFi dependency — it runs off the device's local clock, synced (if at all) via the NTP step described in the parent plan's WiFi section.
