#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include "WifiUtil.h"
#include "DisplayUI.h"
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

  // Cleanly stop the fast-path attempt before handing off to WiFiManager -
  // without this, the STA driver is still "connecting" from the WiFi.begin()
  // above, and WiFiManager's own first (silent) connect attempt gets
  // rejected at the driver level ("sta is connecting, cannot set config")
  // instead of actually being tried - even when it would have used the
  // exact same working credentials.
  WiFi.disconnect(true);
  delay(200);

  // Fall back to the setup portal - bounded, so a device that's never
  // been configured (or whose WiFi stopped working) still finishes
  // booting into the offline card browser rather than hanging here.
  // 25s per connect attempt / 4 min total portal window - generous enough
  // to notice the "ESP PlayGround Setup" network, join it, wait for the
  // captive portal prompt, and type a password once.
  WiFiManager wm;
  wm.setConfigPortalTimeout(WIFI_SETUP_PORTAL_TIMEOUT_SEC);
  wm.setConnectTimeout(25);
  wm.setAPCallback(onSetupPortalStart);

  return wm.autoConnect(WIFI_SETUP_AP_NAME, WIFI_SETUP_AP_PASSWORD);
}
