#pragma once

// Physics/state for the hidden dino-jump game mode. Deliberately has no
// display code in here - same split as Cards.cpp (data) vs DisplayUI.cpp
// (drawing), so the SSD1306 `display` object stays owned by one file.

enum GameRunState {
  GAME_COUNTDOWN,  // "Ready" / "Set" / "Go" intro, nothing moves yet
  GAME_RUNNING,
  GAME_OVER
};

// Only one obstacle is ever in play at a time - it scrolls fully off
// screen, then a gap, then the next spawns. That's a deliberate gameplay
// choice (not a screen-space limitation): sequential single-obstacle
// pacing so each one is clearly anticipatable, rather than two potentially
// overlapping on a 128px-wide screen.
struct GameCactus {
  bool active;
  float x;      // center x, px from left edge; can go negative before deactivating
  bool big;     // bigger variant, same as the ANIM_DINO_RUN card preview - both sizes appear, picked at random per spawn
};

// Read-only snapshot for DisplayUI's renderer - plain fields rather than
// per-value getters since every field is needed together, every frame.
struct GameSnapshot {
  GameRunState state;
  int countdownValue;      // 2/1/0, only meaningful during GAME_COUNTDOWN
  int dinoHeightPx;         // height above ground, 0 = grounded
  int dinoForwardOffsetPx;  // forward nudge during the jump arc, 0 = base x
  bool legFrameA;           // which running-leg pose to draw (ignored mid-air)
  GameCactus cactus;
  int score;
  int bestScore;  // highest score seen since boot - see bestScore in GameEngine.cpp
};

// Resets to a fresh run: GAME_COUNTDOWN state, dino grounded, no cacti,
// score 0. Does NOT touch the best-score high-water mark - that persists
// in RAM across runs (and across mode switches) until the device reboots
// or gets reflashed; there's no flash/EEPROM persistence, by design.
// Called on entering game mode (long-press or remote toggle-on).
void initGame();

// Alias for initGame() - used when a short press on the game-over screen
// starts a new run. Also replays the Ready/Set/Go countdown (see
// docs/desk-mate-project-plan.md game-mode section) rather than dropping
// straight back into running.
void gameRestart();

// Makes the dino jump if it's grounded and the game is actively running -
// a no-op during the countdown or after game over, so a stray press can't
// skip the intro or queue up a jump early. Jump height auto-scales to
// whichever cactus is currently in play (small vs big) - see gameJump()
// in GameEngine.cpp for why a single fixed arc doesn't work for both.
void gameJump();

// Advances physics by however much real time has elapsed since the last
// call (millis()-based, not tick-based) - so the jump arc and scroll speed
// stay consistent regardless of how often the caller invokes this. Caller
// (DisplayUI's updateGameFrame()) is responsible for throttling call
// frequency - see GAME_FRAME_INTERVAL_MS in Config.h.
void stepGame();

bool isGameOver();
const GameSnapshot& getGameSnapshot();
