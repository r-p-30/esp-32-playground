#pragma once

// Tiny wrapper around the ESP32's built-in NVS flash storage (Preferences
// library, already part of the Arduino core - no new dependency, and
// unaffected by the app/SPIFFS partition scheme choice in Config.h/README,
// since NVS is its own separate partition present in every layout) so
// per-game best scores survive a reboot or reflash, not just a run reset
// within the same power-on session. Reads/writes are deliberately rare
// (see callers - loaded once per game module, saved at most once per run,
// only when the run actually beat the previous best) rather than on every
// score change, since flash writes are slow and have a limited write-cycle
// lifetime.

// key must be <=15 chars (Preferences' own NVS key length limit). Returns
// 0 if nothing's been saved yet under this key (first boot, or a key never
// written).
int loadHighScore(const char* key);

// Overwrites the stored value unconditionally - callers are expected to
// have already checked it's actually higher than what's stored (see
// GameEngine.cpp/WhackEngine.cpp), this doesn't re-check.
void saveHighScore(const char* key, int score);
