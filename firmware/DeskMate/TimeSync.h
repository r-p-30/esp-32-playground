#pragma once

// Attempts one brief WiFi connection (bounded by WIFI_CONNECT_TIMEOUT_MS)
// to sync real time via NTP, then disconnects. Always returns - never
// blocks indefinitely, and the device must work fine if this fails.
void syncTime();

// True if syncTime() managed to get a real timestamp this boot.
bool isTimeSynced();
