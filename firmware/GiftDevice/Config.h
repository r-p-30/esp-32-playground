#pragma once

// ---- Pin assignments (matches wiring table in gift-project-plan.md) ----
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
#define ENCODER_DEBOUNCE_MS   2
#define LONG_PRESS_MS          500
#define ANIMATION_DURATION_MS  400

// ---- WiFi / NTP (Phase 1) ----
// TODO: fill in before first boot. A phone hotspot SSID/password works fine for testing.
#define WIFI_SSID       "YOUR_WIFI_OR_HOTSPOT_NAME"
#define WIFI_PASSWORD   "YOUR_PASSWORD"
#define WIFI_CONNECT_TIMEOUT_MS  5000UL

#define NTP_SERVER          "pool.ntp.org"
#define GMT_OFFSET_SEC       (5 * 3600 + 1800)  // TODO: adjust for your timezone (IST default)
#define DAYLIGHT_OFFSET_SEC  0
