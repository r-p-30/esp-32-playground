#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include "WifiUtil.h"
#include "DisplayUI.h"
#include "InputEncoder.h"
#include "Config.h"

// Fires when WiFiManager gives up on known credentials and opens the
// setup portal - lets the OLED explain what's happening instead of
// sitting frozen while wm.autoConnect() blocks.
static void onSetupPortalStart(WiFiManager* wm) {
  showWifiSetupPrompt(WIFI_SETUP_AP_NAME);
}

bool connectWiFiAtBootWithSetupFallback(unsigned long normalTimeoutMs) {
  // Fast path first: reconnect with whatever's already stored in NVS from
  // a past successful connect (portal-configured or otherwise). On a
  // genuinely first-ever boot with empty NVS this just fails immediately
  // and falls through to the setup portal below, which is the intended
  // path for a never-configured device.
  WiFi.mode(WIFI_STA);
  WiFi.begin();

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < normalTimeoutMs) {
    delay(100);
  }
  if (WiFi.status() == WL_CONNECTED) return true;

  // Cleanly stop the fast-path attempt before trying anything else -
  // without this, the STA driver is still "connecting" from the WiFi.begin()
  // above, and the next attempt gets rejected at the driver level
  // ("sta is connecting, cannot set config") instead of actually being
  // tried - even when it would have used the exact same working
  // credentials.
  WiFi.disconnect(true);
  delay(200);

  // Second fast path: hardcoded credentials in WifiCredentials.h, if
  // filled in - skips WiFiManager's captive-portal flow entirely. Kept
  // as an explicit escape hatch alongside the portal (not instead of it)
  // given that flow's real-world flakiness - AP+STA dual-mode connect
  // reliability, phones handling a no-internet AP poorly, etc.
  if (strlen(WIFI_SSID) > 0) {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    unsigned long hcStart = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - hcStart) < normalTimeoutMs) {
      delay(100);
    }
    if (WiFi.status() == WL_CONNECTED) return true;
    WiFi.disconnect(true);
    delay(200);
  }

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

  // Non-blocking mode: autoConnect() kicks the portal off and returns
  // right away instead of blocking here until it connects/fails/times
  // out. That's what lets the loop below poll the encoder for a
  // long-press to bail out early - with the library's own blocking
  // autoConnect(), there'd be no way to skip the wait short of power
  //-cycling the device.
  wm.setConfigPortalBlocking(false);
  wm.autoConnect(WIFI_SETUP_AP_NAME, WIFI_SETUP_AP_PASSWORD);

  unsigned long portalStart = millis();
  unsigned long portalTimeoutMs = (unsigned long)WIFI_SETUP_PORTAL_TIMEOUT_SEC * 1000UL;
  while (true) {
    wm.process();  // services the captive portal's web server/DNS - must be called regularly
    if (WiFi.status() == WL_CONNECTED) return true;
    if ((millis() - portalStart) >= portalTimeoutMs) return false;
    if (pollEncoder() == ENC_LONG_PRESS) {
      wm.stopConfigPortal();
      return false;
    }
    delay(10);
  }
}
