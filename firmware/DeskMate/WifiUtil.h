#pragma once

// Boot-only. Reconnects using whatever credentials WiFi last used
// successfully (bounded by normalTimeoutMs) - works whether that was the
// hardcoded dev seed in WifiCredentials.h or something entered via the
// setup portal, since the ESP32 persists the last successful network
// across reboots regardless of which path set it. If that fails, falls
// back to a WiFiManager captive-portal setup window bounded by
// WIFI_SETUP_PORTAL_TIMEOUT_SEC (Config.h) - you join a temporary
// "ESP PlayGround Setup" network from your phone and pick the real WiFi
// once. Whatever you enter is persisted the same way as any other
// successful WiFi.begin(), so subsequent boots reconnect via the fast
// path without needing this fallback again. If nobody configures it
// before the timeout, returns false and the device continues fully
// offline, same as always - this never blocks indefinitely. The
// connection this establishes is meant to stay up for the device's
// entire runtime (see TimeSync.cpp/DeskMate.ino) - there's no
// disconnect-and-reconnect-later helper, since USB power removed the
// battery reason to ever drop it.
bool connectWiFiAtBootWithSetupFallback(unsigned long normalTimeoutMs);
