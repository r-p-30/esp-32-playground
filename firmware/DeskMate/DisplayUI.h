#pragma once

void initDisplay();

// Redraws the screen for the given card index (0-based).
void showCard(int index);

// Starts the short-press sparkle overlay. Non-blocking - actual drawing
// happens in updateDisplayAnimation().
void triggerAnimation();

// Call every loop() iteration. No-op unless an animation is in flight.
void updateDisplayAnimation();

// Placeholder screen for game mode - the actual game isn't built yet,
// this just gives the long-press toggle somewhere real to land.
void showGamePlaceholder();

// Dims the display and shows the full-screen inverted clock. Call once
// when entering; call renderNightMode() every loop() iteration after
// that (it only actually redraws when the displayed minute changes).
void enterNightMode();
void renderNightMode();

// Restores normal contrast. Caller is responsible for redrawing whatever
// should show next (showCard() or showGamePlaceholder()).
void exitNightMode();

// Brief screen invert, for the remote "identify" ping - doesn't touch
// whatever's currently shown otherwise.
void flashIdentify();

// Shown once, briefly, while the boot-time WiFi setup portal is open
// (WifiUtil.cpp) - so the screen isn't just frozen/blank during that
// window. apName is whatever the user should look for and join.
void showWifiSetupPrompt(const char* apName);

// Shown briefly right after the boot-time connect attempt resolves
// (TimeSync.cpp) - so success/failure is visible on the device itself,
// not just in the serial monitor. Blocks for a couple seconds so there's
// time to actually read it before boot continues into the card browser.
void showWifiConnected(const char* ip);
void showWifiFailed();
