#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include "TimeSync.h"
#include "WifiUtil.h"
#include "DisplayUI.h"
#include "Config.h"

static bool timeSynced = false;

// Howard Hinnant's days-from-civil algorithm: converts a UTC calendar date
// to days since 1970-01-01 without relying on platform timegm() support.
static long daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  const long era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (long)doe - 719468;
}

static time_t tmUtcToEpoch(const struct tm& t) {
  long days = daysFromCivil(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
  return days * 86400L + t.tm_hour * 3600L + t.tm_min * 60L + t.tm_sec;
}

// Parses an HTTP Date header value, e.g. "Wed, 21 Oct 2015 07:28:00 GMT".
static bool parseHttpDate(const String& dateLine, struct tm& outTm) {
  char wkday[4] = {0};
  char monStr[4] = {0};
  int day, year, hh, mm, ss;

  int matched = sscanf(dateLine.c_str(), "%3s, %d %3s %d %d:%d:%d",
                        wkday, &day, monStr, &year, &hh, &mm, &ss);
  if (matched != 7) return false;

  static const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  int month = -1;
  for (int i = 0; i < 12; i++) {
    if (strcmp(monStr, months[i]) == 0) { month = i; break; }
  }
  if (month < 0) return false;

  outTm.tm_year = year - 1900;
  outTm.tm_mon = month;
  outTm.tm_mday = day;
  outTm.tm_hour = hh;
  outTm.tm_min = mm;
  outTm.tm_sec = ss;
  outTm.tm_isdst = 0;
  return true;
}

// Fallback for networks that block outbound NTP (UDP/123) - reads the
// Date header off a plain HTTPS response instead. Accurate to ~1 second,
// which is plenty for "what's today's date" - and HTTPS/443 essentially
// never gets blocked, since that would break the rest of the internet too.
static bool fetchTimeViaHttpsHeader() {
  WiFiClientSecure client;
  client.setInsecure();  // only reading a public header, nothing sensitive
  client.setTimeout(5000);

  if (!client.connect("www.google.com", 443)) {
    return false;
  }

  client.print("HEAD / HTTP/1.1\r\nHost: www.google.com\r\nConnection: close\r\n\r\n");

  unsigned long start = millis();
  String dateLine;
  while (client.connected() && (millis() - start) < 5000) {
    if (client.available()) {
      String line = client.readStringUntil('\n');
      if (line.startsWith("Date: ")) {
        dateLine = line.substring(6);
        dateLine.trim();
        break;
      }
    }
  }
  client.stop();

  if (dateLine.length() == 0) return false;

  struct tm parsedTm = {};
  if (!parseHttpDate(dateLine, parsedTm)) return false;

  // Store true UTC here, not UTC+offset - configTime() (already called in
  // syncTime() before this fallback runs) sets up the TZ mechanism that
  // getLocalTime()/localtime() use to apply GMT_OFFSET_SEC at read time.
  // Adding the offset again here double-counted it (device showed a time
  // 5:30 ahead of actual IST - exactly one extra GMT_OFFSET_SEC).
  time_t utcEpoch = tmUtcToEpoch(parsedTm);
  struct timeval tv = { utcEpoch, 0 };
  settimeofday(&tv, nullptr);
  return true;
}

void syncTime() {
  Serial.print("Connecting to WiFi for time sync");
  // Boot-only: tries known credentials first, falls back to the
  // WiFiManager setup portal (bounded) if that fails - see WifiUtil.h.
  bool connected = connectWiFiAtBootWithSetupFallback(WIFI_CONNECT_TIMEOUT_MS);
  Serial.println();

  if (connected) {
    Serial.print("WiFi connected, IP: ");
    Serial.println(WiFi.localIP());
    showWifiConnected(WiFi.localIP().toString().c_str());
    Serial.println("Syncing time via NTP...");
    // Multiple fallback servers - some networks (mobile hotspots especially)
    // block or drop specific NTP hosts over UDP/123, so trying a few
    // different ones in one call meaningfully improves success odds.
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER, "time.google.com", "time.nist.gov");

    struct tm timeinfo;
    timeSynced = getLocalTime(&timeinfo, 10000);

    if (!timeSynced) {
      Serial.println("NTP failed (likely blocked on this network) - trying HTTPS fallback...");
      timeSynced = fetchTimeViaHttpsHeader();
      Serial.println(timeSynced ? "Time synced via HTTPS fallback." : "HTTPS fallback also failed - continuing offline.");
    } else {
      Serial.println("Time synced via NTP.");
    }

    if (timeSynced) {
      struct tm nowTm;
      if (getLocalTime(&nowTm, 1000)) {
        char buf[32];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &nowTm);
        Serial.print("Current synced time: ");
        Serial.println(buf);
      }
    }
  } else {
    Serial.println("WiFi connect timed out - continuing offline.");
    showWifiFailed();
  }

  // Deliberately left connected (unlike the old battery-conscious Phase 1
  // behavior) - the device is USB-powered now, and RemoteControl.cpp holds
  // a permanent WebSocket connection over this same WiFi session for the
  // rest of the device's runtime. See DeskMate.ino's setup().
}

bool isTimeSynced() {
  return timeSynced;
}
