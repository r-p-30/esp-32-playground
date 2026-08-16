#pragma once

#include "WifiCredentials.h"  // gitignored - see WifiCredentials.example.h

// ---- Pin assignments (matches wiring table in desk-mate-project-plan.md) ----
#define PIN_OLED_SDA   21
#define PIN_OLED_SCL   22
#define PIN_BUZZER     25
#define PIN_ENC_CLK    18
#define PIN_ENC_DT     19
#define PIN_ENC_SW     23

// ---- OLED ----
#define OLED_WIDTH     128
#define OLED_HEIGHT    64
#define OLED_I2C_ADDR  0x3C

// ---- Encoder / button timing ----
#define LONG_PRESS_MS          500
#define VERY_LONG_PRESS_MS     2000
#define ANIMATION_DURATION_MS  1000

// ---- WiFi / NTP (Phase 1) ----
// How long the boot-time fast path (reconnect via NVS-stored credentials -
// see connectWiFiAtBootWithSetupFallback() in WifiUtil.cpp) waits before
// giving up and falling to the setup portal below. WiFi stays connected
// for the device's entire runtime after this (USB-powered, no battery
// reason to drop it) - see DeskMate.ino and RemoteControl.cpp.
#define WIFI_CONNECT_TIMEOUT_MS  5000UL

// ---- WiFi setup portal (Phase 2 - WiFiManager) ----
// Only runs at boot, and only if the normal fast connect (above) fails.
// Bounded so a device that's never configured (or whose WiFi stopped
// working) still finishes booting into the offline card browser instead
// of hanging - see WifiUtil.cpp. 4 minutes - real-world testing showed
// 2 minutes was too tight once you count phone captive-portal detection
// and typing a password once you've joined the setup AP.
// WIFI_SETUP_AP_NAME / WIFI_SETUP_AP_PASSWORD live in WifiCredentials.h
// (gitignored).
#define WIFI_SETUP_PORTAL_TIMEOUT_SEC  240UL

#define NTP_SERVER          "pool.ntp.org"
#define GMT_OFFSET_SEC       (5 * 3600 + 1800)  // TODO: adjust for your timezone (IST default)
#define DAYLIGHT_OFFSET_SEC  0

// ---- Remote control (optional - only does anything once RemoteApi.h has
// real values in it; harmless no-op otherwise) ----
// The device holds one permanent WebSocket connection to the hosted site
// (see RemoteControl.cpp) - state changes push instantly, no polling
// delay. This just controls how often a small "I'm alive" status message
// goes out over that same connection, purely for the site's "last seen"
// display - doesn't affect how fast buzzes/card changes/etc. arrive.
#define REMOTE_HEARTBEAT_INTERVAL_MS  30000UL
