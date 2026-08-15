# ESP32 Desk Mate — Project Plan

**Timeline:** 10 days, evenings only
**Concept:** A desk device with a knob you turn to browse through 10 personal "cards" (text + small pixel drawings), with sound feedback and a couple of hidden interactions.

---

## 1. Parts List (CONFIRMED — all in hand)

| Component | Product | Price | Notes |
|---|---|---|---|
| Microcontroller | Robocraze ESP32 NodeMCU 30 Pin, WiFi/Bluetooth | ₹521 | CP2102 USB-UART chip, Micro USB. **Pins confirmed soldered** (matched physical unit to photo). |
| Display | Robocraze 1.3" I2C OLED, 128x64 | ₹489 | **Pins confirmed soldered** (matched physical unit to photo — this one turned out fine despite the earlier review warning). I2C address confirmed via scan: **0x3C**. |
| Input | Robodo KY-040 Rotary Encoder (Pack of 2) | ₹255 | Has integrated push-button. Pins confirmed usable — wired and tested successfully. |
| Sound | Electronic Spices 5V Passive Piezo Buzzer (Pack of 2) | ₹99 | Bare wire legs (normal, not a soldering defect) — connects fine via female jumper socket or breadboard. Tested working. |
| Wiring | Female-to-female jumper wires (20) | ~₹100 | For direct signal connections (SCL, SDA, CLK, DT, SW, buzzer signal) |
| Wiring | Male-to-female jumper wires (20) | ~₹120 | For component power/ground pins → breadboard rails |
| Wiring | Breadboard | — | Used as shared power/GND hub (ESP32 only has 1× 3V3, 2× GND — not enough for all components individually) |
| Power (portable) | xcluma 3xAA 4.5V battery holder, with switch and cover | ₹79 | Arriving later — will solder red lead to VIN, black lead to GND once it arrives. Not required for initial build/testing (USB power works fine via laptop or wall adapter until then). |

**Total spent:** ~₹1,675 (core electronics) + ~₹300 (wiring/power extras)

---

## 2. Soldering — Resolved

All three main components (ESP32, OLED, encoder) confirmed to have usable soldered pins by comparing the physical units against listing photos. No shop visit needed. Buzzer's bare wire legs are normal and not a soldering issue.

---

## 3. Wiring Table (CONFIRMED, using breadboard as power/GND hub)

ESP32 only has 1× 3V3 and 2× GND pin — not enough for 3 components needing power/ground individually. Fix: route power/GND through breadboard rails, which act as shared connection points.

**Power/GND (via breadboard rails, male-to-female wires):**

| Component | Pin | Connects to | Wire type |
|---|---|---|---|
| ESP32 | 3V3 | Breadboard + rail | male-to-female |
| ESP32 | GND | Breadboard – rail | male-to-female |
| OLED | VCC | Breadboard + rail | male-to-female |
| OLED | GND | Breadboard – rail | male-to-female |
| Encoder | + | Breadboard + rail | male-to-female |
| Encoder | GND | Breadboard – rail | male-to-female |
| Buzzer | – | Breadboard – rail | male-to-female (or leg pushed directly into breadboard) |

**Signal pins (direct to ESP32, female-to-female wires):**

| Component | Pin | ESP32 Pin | Reason |
|---|---|---|---|
| OLED | SCL | GPIO22 | I2C clock pin (confirmed working) |
| OLED | SDA | GPIO21 | I2C data pin (confirmed working) |
| Buzzer | + | GPIO25 | Digital output (confirmed working) |
| Encoder | CLK | GPIO18 | Rotation signal A (confirmed working) |
| Encoder | DT | GPIO19 | Rotation signal B (confirmed working) |
| Encoder | SW | GPIO23 | Push-button (confirmed working) |

**Later (once battery pack arrives):** solder battery holder's red lead to VIN, black lead to GND (can share a GND pin/joint with an existing wire).

Avoid GPIO 0, 2, 15 (boot-mode pins) and GPIO 6–11 (internal flash) for any custom wiring — not used in this build, noted for reference.

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

The ESP32 supports WiFi/Bluetooth, but the core device (phase 1) does NOT depend on it — it must work fully offline. WiFi is layered in across later phases.

**Phase 1 (ship within 10 days) — offline-first, one light WiFi feature:**
- Card-cycling core — fully offline, always works
- NTP time sync (get real current date/time on boot) — connects briefly using **hardcoded WiFi credentials**, then device tracks elapsed time locally without needing a continuous connection
- **No home WiFi needed for testing** — a phone hotspot works identically to a router as far as the ESP32 is concerned (it's just an SSID + password either way). Use hotspot credentials temporarily during dev/testing.
- **Mandatory graceful fallback**: if WiFi connection fails or times out (~5 sec attempt), device proceeds in offline mode using its last-known/internal clock — must never block or break the core experience
- Code written **modularly** (e.g. separate `getCardContent()`, `checkForSync()`, `syncTime()` functions) so phase 2 can fill in real network calls later without a full rewrite — a re-upload of new code will still be required when phase 2 is ready, but the structure won't need to change

**Phase 1.1 (post-build, hardware only, no code changes) — battery connection:**
- 3xAA battery holder (with switch) arrives later — not required for phase 1, USB power works fine until then
- Solder red lead → VIN, black lead → GND (can share a GND pin/joint with an existing wire)
- No firmware changes needed — VIN accepts the ~4.5V from 3 AA batteries directly, same as USB's 5V
- Makes the device portable/cordless — do this once, whenever convenient
- Enclosure must stay accessible (not fully sealed) until this step is done

**Phase 2 (post-build, possibly built collaboratively later):**
- Small self-hosted config site: add image + text, live preview based on screen size, configure animation, preview that too
- Device fetches card content from this hosted endpoint on a schedule → this is the "remote message updates" feature
- Sync/notification trigger: send a message/ping from phone → device buzzes + shows it live
- Swap hardcoded WiFi credentials for **WiFiManager** (a standard ESP32/Arduino library): if the device can't connect on boot, it becomes a temporary hotspot with a captive portal — open it on your phone, pick your own WiFi, enter the password once. Solves needing to hardcode a password, and survives the router changing.
- Consider a shared GitHub repo at this point for collaborative iteration

**Phase 3 (stretch, if both want to keep going):** hidden game mode — see section 6.

---

## 6. Stretch Goal (only if time allows): Hidden Game Mode

- Dino-jump-style game as an easter egg, same hardware, separate code path
- Trigger idea: hold the encoder button for ~5 seconds at boot to launch game mode instead of the card browser
- Controls: press = jump; obstacles scroll toward the dino; collision = game over + buzzer fail sound
- Target: 15–20 fps is achievable and sufficient for this game type; use partial-redraw (not full-screen clear each frame) for responsiveness
- Explicitly deprioritized — only attempted after the core build (steps 1–7 below) is fully working with buffer days to spare

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

## 8. Sanity Checks — ALL PASSED

**Check 1 — Toolchain: PASSED**
- Arduino IDE + ESP32 board support (Espressif package, v3.3.11) installed successfully after working through a GitHub download failure (fixed via retry + IDE restart)
- Blink test confirmed working

**Check 2 — OLED: PASSED**
- Wired via breadboard rails (power) + direct GPIO21/22 (I2C signal)
- I2C scan confirmed device found at address **0x3C**
- Adafruit SSD1306 + Adafruit GFX libraries installed, demo sketch displays correctly on screen

**Check 3 — Buzzer + Encoder: PASSED**
- Buzzer: confirmed working via `tone()`/`noTone()` test on GPIO25, now set to silent/idle state
- Encoder: rotation + button both confirmed working. Final tested code below — **quadrature state-table decoding** (not simple edge detection) was needed to eliminate false direction flips from mechanical bounce; button uses hold-duration measured on release to distinguish short vs. long press, with a debounce gap to prevent double-counting from switch chatter.

```cpp
int clkPin = 18;
int dtPin = 19;
int swPin = 23;

int counter = 0;
byte oldAB = 0;

int8_t table[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};

bool wasPressed = false;
unsigned long pressStartTime = 0;
unsigned long lastReleaseTime = 0;
int longPressThreshold = 500; // ms
int debounceGap = 250;        // ms, ignore releases closer together than this

void setup() {
  Serial.begin(115200);
  pinMode(clkPin, INPUT);
  pinMode(dtPin, INPUT);
  pinMode(swPin, INPUT_PULLUP);
}

void loop() {
  oldAB <<= 2;
  oldAB |= (digitalRead(clkPin) << 1) | digitalRead(dtPin);
  int8_t change = table[oldAB & 0x0F];

  if (change != 0) {
    counter += change;
    Serial.println(counter / 2); // divide by 2: each detent produces 2 valid transitions
  }

  bool isPressed = (digitalRead(swPin) == LOW);

  if (isPressed && !wasPressed) {
    pressStartTime = millis();
  }

  if (!isPressed && wasPressed) {
    if (millis() - lastReleaseTime > debounceGap) {
      unsigned long heldFor = millis() - pressStartTime;
      if (heldFor >= longPressThreshold) {
        Serial.println("Long press");
      } else {
        Serial.println("Short press");
      }
      lastReleaseTime = millis();
    }
  }

  wasPressed = isPressed;
}
```

**Next step:** combine OLED + encoder + buzzer into one sketch (all three wired simultaneously via breadboard), confirm no conflicts, then build the actual card-cycling logic using this encoder code as the input-handling foundation.

---

## 9. Core Concepts Reference (for your own understanding)

- **Voltage/Current/GND:** ESP32 logic runs at 3.3V (not 5V like old Arduino Uno). Every component's GND must connect to the same common ground.
- **GPIO pins:** general-purpose; you assign their role (input/output) in code. A few are reserved (boot pins, flash pins) — avoid those.
- **I2C:** OLED uses 2 signal wires (SCL, SDA) + power + GND. Multiple I2C devices can share the same 2 wires via unique addresses.
- **Rotary encoder mechanics:** CLK and DT pins trigger in a specific order depending on spin direction — a library interprets this into "turned clockwise/counter-clockwise." SW is a separate, simple push-button on the same unit.
- **Code structure:** `setup()` runs once (initialize pins, start display), `loop()` runs continuously (check for encoder/button changes, update display, buzz).

---

## 10. Open Decisions

- ~~OLED size~~ — resolved, 1.3" confirmed working
- ~~Soldering~~ — resolved, all pins confirmed usable, no shop visit needed
- ~~Power for portability~~ — resolved, see Phase 1.1 in section 5
- Exact wording/content for the 10 cards — not started yet, no hardware dependency, can be drafted anytime
- Long-press behavior: replay sound vs. jump to card 1 — undecided
- Whether to attempt the stretch game mode at all — decide only after core build is confirmed working
- Phase 2 timing: may happen before or after the initial build phase, possibly collaboratively — not yet decided
- Phase 2 hosting choice for the config site/API endpoint — not yet decided
