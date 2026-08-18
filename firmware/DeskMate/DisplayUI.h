#pragma once

void initDisplay();

// Redraws the screen for the given card index (0-based).
void showCard(int index);

// Starts the short-press sparkle overlay. Non-blocking - actual drawing
// happens in updateDisplayAnimation().
void triggerAnimation();

// Call every loop() iteration. No-op unless an animation is in flight.
void updateDisplayAnimation();

// Draws whatever GameEngine's current snapshot is, immediately, without
// advancing physics - call once right after initGame()/gameRestart() so
// entering game mode (or restarting after game over) shows a fresh frame
// without waiting for the next throttled updateGameFrame() tick.
void showGameFrame();

// Draws the game-picker menu (Game.h's GAMES[]), one row per entry, arrow
// beside whichever index is selected. Call once on entering the menu and
// again on every ENC_NEXT/ENC_PREV while in it - no continuous animation,
// so unlike showGameFrame() there's no throttled per-loop counterpart.
void showGameMenu(int selectedIndex);

// Call every loop() iteration while in game mode. Throttles itself to
// GAME_FRAME_INTERVAL_MS (Config.h) - each time it fires, advances
// GameEngine's physics one step and redraws. Deliberately slow so the
// game stays reactable-to on real hardware.
void updateGameFrame();

// Same pair of calls as showGameFrame()/updateGameFrame() above, but for
// the whack-a-mole game mode (WhackEngine.h) - kept as separate functions
// rather than parameterizing the Dino ones, since the two games' snapshots
// (GameSnapshot vs WhackSnapshot) and scenes share no drawing code.
void showMoleFrame();
void updateMoleFrame();

// Same pair of calls as showGameFrame()/showMoleFrame() above, but for the
// snake game mode (SnakeEngine.h).
void showSnakeFrame();
void updateSnakeFrame();

// Same pair of calls again, but for the Tetris game mode (TetrisEngine.h) -
// see docs/tetris-implementation-plan.md for the 90-degree screen-rotation
// mapping this renderer applies that none of the other games need.
void showTetrisFrame();
void updateTetrisFrame();

// Same pair of calls again, but for the Car Racing game mode
// (RacingEngine.h) - reuses Tetris's 90-degree rotation mapping (same
// direction), see docs/racing-implementation-plan.md.
void showRacingFrame();
void updateRacingFrame();

// Dims the display and shows the full-screen inverted clock. Call once
// when entering; call renderNightMode() every loop() iteration after
// that (it only actually redraws when the displayed minute changes).
void enterNightMode();
void renderNightMode();

// Restores normal contrast. Caller is responsible for redrawing whatever
// should show next (showCard() or showGameFrame()).
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
