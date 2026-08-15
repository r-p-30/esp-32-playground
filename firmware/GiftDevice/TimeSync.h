#pragma once

// Attempts one brief WiFi connection (bounded by WIFI_CONNECT_TIMEOUT_MS)
// to sync real time via NTP, then disconnects. Always returns - never
// blocks indefinitely, and the device must work fine if this fails.
void syncTime();

// True if syncTime() managed to get a real timestamp this boot.
bool isTimeSynced();

// Phase 2 stub: intentionally empty for now. This is the seam where a
// scheduled check-in with the hosted config site will be added later,
// without needing to restructure the rest of the sketch.
void checkForSync();
