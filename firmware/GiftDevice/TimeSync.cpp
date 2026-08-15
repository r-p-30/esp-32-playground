#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include "TimeSync.h"
#include "Config.h"

static bool timeSynced = false;

void syncTime() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to WiFi for time sync");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_CONNECT_TIMEOUT_MS) {
    delay(100);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected, syncing time via NTP...");
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);

    struct tm timeinfo;
    timeSynced = getLocalTime(&timeinfo, 5000);
    Serial.println(timeSynced ? "Time synced." : "Connected but NTP sync failed - continuing offline.");
  } else {
    Serial.println("WiFi connect timed out - continuing offline.");
  }

  // Phase 1 only needs a brief connection at boot, per the plan - drop it
  // afterwards rather than staying associated for no reason.
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

bool isTimeSynced() {
  return timeSynced;
}

void checkForSync() {
  // Intentionally empty in Phase 1.
}
