#pragma once

// Copy this file to WifiCredentials.h (same folder) and fill in real values.
// WifiCredentials.h is gitignored - it never gets committed.
// A phone hotspot SSID/password works identically to a home router here.

// Optional - tried right after the NVS fast path and before the setup
// portal broadcasts (connectWiFiAtBootWithSetupFallback() in WifiUtil.cpp).
// Leave both as "" to skip straight to the portal. Fill these in to bypass
// WiFiManager's captive-portal flow entirely on your own bench/dev network.
#define WIFI_SSID       ""
#define WIFI_PASSWORD   ""

// Name/password for the temporary setup portal AP (WifiUtil.cpp) - this is
// how the device joins WiFi if the fast paths above don't work.
#define WIFI_SETUP_AP_NAME      "YOUR_SETUP_AP_NAME"
#define WIFI_SETUP_AP_PASSWORD  "YOUR_SETUP_AP_PASSWORD"
