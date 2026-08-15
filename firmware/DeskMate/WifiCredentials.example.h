#pragma once

// Copy this file to WifiCredentials.h (same folder) and fill in real values.
// WifiCredentials.h is gitignored - it never gets committed.
// A phone hotspot SSID/password works identically to a home router here.

// Name/password for the temporary setup portal AP (WifiUtil.cpp) - this is
// how the device joins WiFi the first time (or any time the last-connected
// network stops working). No separate WIFI_SSID/WIFI_PASSWORD needed here -
// the boot fast path just reconnects with whatever's already stored in NVS
// from a past successful connect (see connectWiFiAtBootWithSetupFallback()).
#define WIFI_SETUP_AP_NAME      "YOUR_SETUP_AP_NAME"
#define WIFI_SETUP_AP_PASSWORD  "YOUR_SETUP_AP_PASSWORD"
