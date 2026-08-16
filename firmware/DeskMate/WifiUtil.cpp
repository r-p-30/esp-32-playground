#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include "WifiUtil.h"
#include "DisplayUI.h"
#include "InputEncoder.h"
#include "Config.h"

// Fires when the setup portal AP starts broadcasting - lets the OLED
// explain what's happening instead of sitting frozen/blank.
static void onSetupPortalStart(WiFiManager* wm) {
  showWifiSetupPrompt(WIFI_SETUP_AP_NAME);
}

bool connectWiFiAtBootWithSetupFallback(unsigned long normalTimeoutMs) {
  WiFi.mode(WIFI_STA);

  // Fast path first: hardcoded credentials in WifiCredentials.h, if
  // filled in - tried immediately on a fresh radio state, before
  // anything else touches it. Order matters here: this used to be the
  // very first thing tried (and reliably worked); it regressed when an
  // NVS-blank attempt got added in front of it, leaving the WiFi driver
  // in a worse state by the time this ran. Skips WiFiManager's
  // captive-portal flow entirely when it works - kept as an explicit
  // escape hatch alongside the portal (not instead of it), given that
  // flow's real-world flakiness (AP+STA dual-mode reliability, phones
  // handling a no-internet AP poorly, etc.).
  if (strlen(WIFI_SSID) > 0) {
    Serial.println("WiFi: trying hardcoded credentials...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    unsigned long hcStart = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - hcStart) < normalTimeoutMs) {
      // Poll for long-press here too, not just in the portal loop below -
      // without this, a long-press during this stretch (and the NVS
      // attempt after it) is silently dropped, and "skip" only starts
      // working once the portal is already up, many seconds later.
      if (pollEncoder() == ENC_LONG_PRESS) {
        Serial.println("WiFi: skipped via long-press (during hardcoded attempt).");
        WiFi.disconnect(true);
        delay(200);
        return false;
      }
      delay(100);
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("WiFi: connected via hardcoded credentials.");
      return true;
    }
    Serial.println("WiFi: hardcoded credentials failed.");

    // Cleanly stop before trying anything else - without this, the STA
    // driver is still "connecting" from the WiFi.begin() above, and the
    // next attempt gets rejected at the driver level ("sta is connecting,
    // cannot set config") instead of actually being tried.
    WiFi.disconnect(true);
    delay(200);
  }

  // Second fast path: reconnect with whatever's already stored in NVS
  // from a past successful connect (most likely a prior WiFiManager
  // portal run) - covers a recipient's device, which has no hardcoded
  // credentials at all. On a genuinely first-ever boot with empty NVS
  // this just fails immediately and falls through to the setup portal
  // below, which is the intended path for a never-configured device.
  Serial.println("WiFi: trying stored (NVS) credentials...");
  WiFi.begin();

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < normalTimeoutMs) {
    if (pollEncoder() == ENC_LONG_PRESS) {
      Serial.println("WiFi: skipped via long-press (during stored-credentials attempt).");
      WiFi.disconnect(true);
      delay(200);
      return false;
    }
    delay(100);
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi: connected via stored credentials.");
    return true;
  }
  Serial.println("WiFi: stored credentials failed - opening setup portal.");

  WiFi.disconnect(true);
  delay(200);

  // Fall back to the setup portal - bounded, so a device that's never
  // been configured (or whose WiFi stopped working) still finishes
  // booting into the offline card browser rather than hanging here.
  // 45s per connect attempt / 4 min total portal window. The connect
  // attempt itself runs with the ESP32 in AP+STA mode (SoftAP still up
  // to serve the portal page while it tries the new network as a
  // station on the same radio) - that dual-mode join is measurably
  // slower and flakier than a normal single-mode connect, so this needs
  // real headroom rather than a tight timeout tuned for the simple case.
  WiFiManager wm;
  wm.setConfigPortalTimeout(WIFI_SETUP_PORTAL_TIMEOUT_SEC);
  wm.setConnectTimeout(45);
  wm.setAPCallback(onSetupPortalStart);

  // startConfigPortal() (not autoConnect()) - we've already tried the
  // stored NVS credentials ourselves just above, so autoConnect()'s own
  // internal "try saved AP first" step would just repeat that same
  // attempt, blocking for another wm.setConnectTimeout(45) seconds (and
  // not pollable - it happens inside the library call, before the loop
  // below ever runs) before finally opening the portal. startConfigPortal()
  // skips straight to broadcasting the AP.
  //
  // Non-blocking mode: kicks the portal off and returns right away
  // instead of blocking here until it connects/fails/times out. That's
  // what lets the loop below poll the encoder for a long-press to bail
  // out early - with the library's own blocking call, there'd be no way
  // to skip the wait short of power-cycling the device.
  wm.setConfigPortalBlocking(false);
  wm.startConfigPortal(WIFI_SETUP_AP_NAME, WIFI_SETUP_AP_PASSWORD);
  Serial.println("WiFi: setup portal AP is up.");

  unsigned long portalStart = millis();
  unsigned long portalTimeoutMs = (unsigned long)WIFI_SETUP_PORTAL_TIMEOUT_SEC * 1000UL;
  while (true) {
    wm.process();  // services the captive portal's web server/DNS - must be called regularly
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("WiFi: connected via setup portal.");
      return true;
    }
    if ((millis() - portalStart) >= portalTimeoutMs) {
      Serial.println("WiFi: setup portal timed out - continuing offline.");
      // wm.stopConfigPortal() (also called on the long-press-skip path
      // below) can leave the STA driver stuck mid-"connecting" instead of
      // cleanly idle - without resetting it here, every later
      // WiFi.reconnect() from the main loop's watchdog (DeskMate.ino)
      // fails forever with "sta is connecting, return error" every cycle.
      wm.stopConfigPortal();
      WiFi.disconnect(true);
      delay(200);
      return false;
    }
    if (pollEncoder() == ENC_LONG_PRESS) {
      Serial.println("WiFi: skipped via long-press (during setup portal) - continuing offline.");
      wm.stopConfigPortal();
      WiFi.disconnect(true);
      delay(200);
      return false;
    }
    delay(10);
  }
}
