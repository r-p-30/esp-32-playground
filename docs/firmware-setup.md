# Firmware Setup — New Machine

Everything needed to get `firmware/DeskMate/DeskMate.ino` compiling and flashing from a machine that's never built this project before — IDE, board support, libraries, and the arduino-cli equivalent. Hardware/wiring lives in [card-mode-plan.md](card-mode-plan.md); this doc is purely the toolchain.

---

## 1. USB driver (Windows, common first gotcha)

The board's CP2102 USB-UART chip needs a driver before its COM port shows up at all. Windows sometimes auto-installs it, sometimes doesn't — if Device Manager doesn't show a COM port with the board plugged in, install the [Silicon Labs CP210x driver](https://www.silabs.com/developer-tools/usb-to-uart-bridge-vcp-drivers) and re-plug the board.

## 2. Arduino IDE + ESP32 board support

1. Install [Arduino IDE 2.x](https://www.arduino.cc/en/software).
2. File → Preferences → **Additional Board Manager URLs**, add:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. Tools → Board → Boards Manager → search **esp32** → install **"esp32 by Espressif Systems"** (verified working against **3.3.11**; a newer version will likely work too, but that's the one this was last compiled against).
4. Tools → Board → select **"ESP32 Dev Module"**.

## 3. Board settings (Tools menu, every time you open the sketch on a new machine)

- **Partition Scheme → "Huge APP (3MB No OTA/1MB SPIFFS)"** — required, not optional. The WebSockets library pushes flash usage past the default scheme's limit. Changing this reshuffles flash layout, so the first flash after switching schemes may force a fresh WiFiManager setup even on a previously-configured board.
- **Port** — whichever COM/tty the board enumerates as once the driver above is installed.

## 4. Libraries

Sketch → Include Library → Manage Libraries, install each of these. Versions below are what this was last verified compiling clean against (via a clean `arduino-cli` build) — newer versions will likely work too, but if something breaks, pinning to these is the known-good baseline:

| Library | Version | Notes |
|---|---|---|
| Adafruit SSD1306 | 2.5.17 | OLED driver |
| Adafruit GFX Library | 1.12.6 | Pulls in **Adafruit BusIO** (1.17.4) as its own dependency — installs automatically |
| ArduinoJson | 7.4.3 | Remote-control JSON parsing |
| WiFiManager | 2.0.17 (by tzapu) | Captive-portal WiFi setup |
| WebSockets | 2.7.2 (by Markus Sattler / Links2004) | Search "WebSockets" in Library Manager — this is the `Links2004/arduinoWebSockets` library, not a similarly-named alternative |

## 5. Per-machine config files (gitignored — recreate these on every new machine, never committed)

1. Copy `firmware/DeskMate/WifiCredentials.example.h` → `WifiCredentials.h` (same folder) and fill in:
   - `WIFI_SSID` / `WIFI_PASSWORD` — optional. Leave both `""` to rely on WiFiManager's setup portal at boot; fill them in to bypass the portal entirely on a known bench/dev network.
   - `WIFI_SETUP_AP_NAME` / `WIFI_SETUP_AP_PASSWORD` — name/password for the device's own temporary setup-portal AP (how it's configured if the fast paths above don't apply).
2. Optional — copy `firmware/DeskMate/RemoteApi.example.h` → `RemoteApi.h` and fill in the companion site's `wss://` endpoint + shared `DEVICE_API_KEY` **only if** you want remote control enabled. The device works fully offline without this file.

## 6. Flash it

Open `firmware/DeskMate/DeskMate.ino` in the IDE and hit Upload.

---

## 7. arduino-cli equivalent

If you'd rather script this than click through the IDE:

```powershell
# Install arduino-cli, then:
arduino-cli config init
arduino-cli config add board_manager.additional_urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32

arduino-cli lib install "Adafruit SSD1306" "Adafruit GFX Library" ArduinoJson WiFiManager WebSockets

# Copy + fill in WifiCredentials.h (and optionally RemoteApi.h) as in step 5 above -
# arduino-cli doesn't do this part for you.

# Find the board's port:
arduino-cli board list

# Compile (this is the same command used to sanity-check changes without flashing):
arduino-cli compile --fqbn "esp32:esp32:esp32:PartitionScheme=huge_app" firmware/DeskMate

# Flash (replace COM3 with whatever `board list` reported):
arduino-cli upload -p COM3 --fqbn "esp32:esp32:esp32:PartitionScheme=huge_app" firmware/DeskMate
```

## 8. First boot after flashing

If `WifiCredentials.h` has empty `WIFI_SSID`/`WIFI_PASSWORD`, the device opens its own WiFiManager setup-portal AP on boot — connect to it from a phone and pick the real network. A long-press on the encoder during this window skips straight to fully offline mode instead, if you don't want to configure WiFi right now — see [desk-mate-project-plan.md](desk-mate-project-plan.md)'s WiFi & Remote Control section for the full boot-connect sequence.
