# ESP32 Gift Project — Plan

**Timeline:** 10 days, evenings only
**Concept:** A desk device with a knob you turn to browse through 10 personal "cards" (text + small pixel drawings), with sound feedback and a couple of hidden interactions.

---

## 1. Parts List (in cart)

| Component | Product | Price | Notes |
|---|---|---|---|
| Microcontroller | Robocraze ESP32 NodeMCU 30 Pin, WiFi/Bluetooth | ₹521 | CP2102 USB-UART chip, Micro USB, pins pre-soldered (confirmed via photo) |
| Display | Robocraze 1.3" I2C OLED, 128x64 | ₹489 | **Pins NOT pre-soldered** — confirmed via reviews despite product photo. Needs local soldering. |
| Input | Robodo KY-040 Rotary Encoder (Pack of 2) | ₹255 | Has integrated push-button. Pin-soldering status unconfirmed — check reviews before finalizing. |
| Sound | Electronic Spices 5V Passive Piezo Buzzer (Pack of 2) | ₹99 | Passive type — supports tone variation |
| Wiring | Female-to-female jumper wires | ~₹100 | Breadboard not required — every connection is component-to-ESP32 direct |

**Subtotal (cart so far):** ~₹1,676 for 5 items (some qty 2, spares included)

**Action before finalizing:** Confirm soldering status on OLED and encoder via seller reviews (search reviews for the word "solder"), not product photos or Rufus AI summaries — both have proven unreliable on this specific detail.

---

## 2. Known Issue: Soldering

- OLED confirmed via reviews: photo shows soldered pins, but actual unit ships **without** pins soldered and **without** loose pins included.
- Plan: take board + a header pin strip to a local electronics/mobile repair shop once delivered. Cost ~₹20–50, ~10–15 min, same-day turnaround typical.
- Check the KY-040 encoder the same way (same seller pattern) before assuming it's fine.
- Alternative: reorder from a listing with reviewer-confirmed pre-soldered pins if the errand risks the timeline.

---

## 3. Wiring Table

| Component | Pin | ESP32 Pin | Reason |
|---|---|---|---|
| OLED | VCC | 3V3 | Power |
| OLED | GND | GND | Ground |
| OLED | SCL | GPIO22 | Default I2C clock pin |
| OLED | SDA | GPIO21 | Default I2C data pin |
| Buzzer | + | GPIO25 | General-purpose output |
| Buzzer | – | GND | Ground |
| Encoder | + | 3V3 | Power |
| Encoder | GND | GND | Ground |
| Encoder | CLK | GPIO18 | Rotation signal A |
| Encoder | DT | GPIO19 | Rotation signal B |
| Encoder | SW | GPIO23 | Push-button |

Avoid GPIO 0, 2, 15 (boot-mode pins) and GPIO 6–11 (internal flash) for any custom wiring.
Verify printed pin labels against this table once the board is in hand — minor differences between batches are possible; update pin numbers in code if so (one-line change).

---

## 4. Functionality Spec

**Turning the knob:**
- Cycles forward/backward through 10 cards (mix of text + small pixel drawings)
- Clockwise = next, counter-clockwise = previous, wraps around at both ends
- Short buzzer beep on every card change

**Pressing the knob (button):**
- **Short press (<500ms):** plays a small animation overlay on the current card (e.g. pulse/sparkle) — shared across all cards for simplicity
- **Long press (>500ms):** replays the card's buzzer sound pattern (or jumps to card 1 as "home" — TBD)
- Configurable in code as a list, so adding a 3rd/4th action later is easy

**Content scope:** 10 cards total. Each needs: short text (fits ~4–6 lines at 128x64), optional small bitmap icon (16x16 or 32x32, convertible from any image via tools like `image2cpp`), and buzzer pattern (can share one across all cards).

---

## 5. WiFi Roadmap (phased)

The ESP32 supports WiFi/Bluetooth, but the core gift (phase 1) does NOT depend on it — it must work fully offline. WiFi is layered in across later phases.

**Phase 1 (ship within 10 days) — offline-first, one light WiFi feature:**
- Card-cycling core — fully offline, always works
- NTP time sync (get real current date/time on boot) — connects briefly using **hardcoded WiFi credentials**, then device tracks elapsed time locally without needing a continuous connection
- **No home WiFi needed for testing** — a phone hotspot works identically to a router as far as the ESP32 is concerned (it's just an SSID + password either way). Use hotspot credentials temporarily during dev/testing.
- **Mandatory graceful fallback**: if WiFi connection fails or times out (~5 sec attempt), device proceeds in offline mode using its last-known/internal clock — must never block or break the core experience
- Code written **modularly** (e.g. separate `getCardContent()`, `checkForSync()`, `syncTime()` functions) so phase 2 can fill in real network calls later without a full rewrite — a re-upload of new code will still be required when phase 2 is ready, but the structure won't need to change

**Phase 2 (post-gift, possibly built together with friend since they're also a dev):**
- Small self-hosted config site: add image + text, live preview based on screen size, configure animation, preview that too
- Device fetches card content from this hosted endpoint on a schedule → this is the "remote message updates" feature
- Sync/notification trigger: send a message/ping from phone → device buzzes + shows it live
- Swap hardcoded WiFi credentials for **WiFiManager** (a standard ESP32/Arduino library): if the device can't connect on boot, it becomes a temporary hotspot with a captive portal — friend opens it on their phone, picks their own WiFi, enters password once. Solves needing to know their password, and survives their router changing.
- Consider a shared GitHub repo at this point, since the friend may want to hack on this together

**Phase 3 (stretch, if both want to keep going):** hidden game mode — see section 6.

---

## 6. Stretch Goal (only if time allows): Hidden Game Mode

- Dino-jump-style game as an easter egg, same hardware, separate code path
- Trigger idea: hold the encoder button for ~5 seconds at boot to launch game mode instead of the card gift
- Controls: press = jump; obstacles scroll toward the dino; collision = game over + buzzer fail sound
- Target: 15–20 fps is achievable and sufficient for this game type; use partial-redraw (not full-screen clear each frame) for responsiveness
- Explicitly deprioritized — only attempted after the core gift (steps 1–7 below) is fully working with buffer days to spare

---

## 7. Evening-by-Evening Plan

| Day | Task |
|---|---|
| 1–2 | Order parts (if not done); once arrived, set up Arduino IDE + ESP32 board support + drivers; run a basic blink test to confirm toolchain |
| 3–4 | Get OLED wired and displaying static "Hello World" text (confirms I2C wiring) |
| 5 | Get buzzer beeping on a timer (confirms digital output); get encoder rotation + button press reading correctly in Serial Monitor |
| 6 | Combine all three components into one sketch; build card-cycling logic (knob → next/prev card index) |
| 7 | Add short-press/long-press logic; write and load the 10 messages/bitmaps |
| 8 | Enclosure/packaging; **stretch: start game mode if ahead of schedule** |
| 9 | Polish, test full run-through, fix rough edges |
| 10 | Buffer day |

---

## 8. Sanity Checks (do these BEFORE building the real project)

Do these three, one at a time, in separate small sketches — not combined. Each isolates one subsystem, so if something fails you know exactly where to debug instead of untangling 80 lines of combined code.

**Check 1 — Toolchain works**
- Goal: confirm Arduino IDE, ESP32 board support, USB driver, and upload process all work
- Method: install Arduino IDE → add ESP32 board manager URL → install CP2102 driver if not auto-detected → select correct board + COM port → upload the built-in `Blink` example (File → Examples → 01.Basics → Blink), pointed at the ESP32's onboard LED pin (commonly GPIO2, confirm against your specific board)
- Pass condition: onboard LED blinks on/off in a steady rhythm

**Check 2 — OLED wiring + I2C works**
- Goal: confirm the 4-wire I2C connection (VCC, GND, SCL→GPIO22, SDA→GPIO21) is correct and the display library is set up right
- Method: install the `Adafruit SSD1306` + `Adafruit GFX` libraries (or `U8g2` — pick one, U8g2 has broader driver support if the 1.3" turns out to use a different chip than expected) → run a basic "Hello World" example from the library, adjusting the screen resolution/address in code to match your display (128x64, address usually `0x3C` — check via an I2C scanner sketch if unsure)
- Pass condition: text appears correctly on the OLED screen

**Check 3 — Buzzer + encoder digital I/O works**
- Goal: confirm basic digital output (buzzer) and digital input (encoder rotation + button) both work
- Method (buzzer): simple sketch that sets the buzzer's GPIO HIGH, waits, LOW, waits, on a loop — or use `tone()` for an actual pitch if using a passive buzzer
- Method (encoder): simple sketch that reads CLK/DT/SW pins with `Serial.print()`, turn the knob and press the button, watch the values change in the Serial Monitor
- Pass condition: buzzer audibly beeps; Serial Monitor shows changing values as you turn/press the encoder

**After all three pass individually** → combine into one sketch with all three components initialized together, confirm no conflicts (e.g. shared pins, power draw issues) before writing the actual card-cycling game logic.

---

## 9. Core Concepts Reference (for your own understanding)

- **Voltage/Current/GND:** ESP32 logic runs at 3.3V (not 5V like old Arduino Uno). Every component's GND must connect to the same common ground.
- **GPIO pins:** general-purpose; you assign their role (input/output) in code. A few are reserved (boot pins, flash pins) — avoid those.
- **I2C:** OLED uses 2 signal wires (SCL, SDA) + power + GND. Multiple I2C devices can share the same 2 wires via unique addresses.
- **Rotary encoder mechanics:** CLK and DT pins trigger in a specific order depending on spin direction — a library interprets this into "turned clockwise/counter-clockwise." SW is a separate, simple push-button on the same unit.
- **Code structure:** `setup()` runs once (initialize pins, start display), `loop()` runs continuously (check for encoder/button changes, update display, buzz).

---

## 10. Open Decisions (finalize once parts arrive)

- Final OLED size confirmed as 1.3" (in cart) — could reconsider 2.42" if delivery timing allows, purely optional
- Exact wording/content for the 10 cards — not started yet, no hardware dependency, can be drafted anytime
- Long-press behavior: replay sound vs. jump to card 1 — undecided
- Whether to attempt the stretch game mode at all — decide only after core build (day 7) is confirmed working
- Phase 2 timing: may happen before or after gifting, possibly collaboratively with friend — not yet decided
- Phase 2 hosting choice for the config site/API endpoint — not yet decided
