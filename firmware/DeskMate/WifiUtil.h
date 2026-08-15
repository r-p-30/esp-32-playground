#pragma once

// Reconnects using whatever credentials WiFi last used successfully -
// works whether that was the hardcoded dev seed in WifiCredentials.h or
// something entered via the setup portal (see below); the
// ESP32 persists the last successful network across reboots regardless
// of which path set it. Bounded by timeoutMs. Caller should call
// disconnectWiFi() afterwards to drop the connection and save power.
bool connectWiFiBriefly(unsigned long timeoutMs);

void disconnectWiFi();

// Boot-only. Tries the normal fast path first (same as connectWiFiBriefly,
// bounded by normalTimeoutMs); if that fails, falls back to a WiFiManager
// captive-portal setup window bounded by WIFI_SETUP_PORTAL_TIMEOUT_SEC
// (Config.h) - you join a temporary "ESP PlayGround Setup"
// network from your phone and pick the real WiFi once. Whatever you
// enter is persisted the same way as any other successful WiFi.begin(),
// so subsequent boots and the routine connectWiFiBriefly() reconnects
// just work without needing this fallback again. If nobody configures it
// before the timeout, returns false and the device continues fully
// offline, same as always - this never blocks indefinitely.
bool connectWiFiAtBootWithSetupFallback(unsigned long normalTimeoutMs);
