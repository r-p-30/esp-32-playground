#pragma once

#include "Config.h"  // RACING_ROWS/RACING_MAX_OBSTACLES/RACING_LANES - needed here, not just the .cpp, since they size arrays below

// Physics/state for the Car Racing game mode. Deliberately has no display
// code in here - same split as GameEngine.h (Dino)/SnakeEngine.h/
// TetrisEngine.h vs DisplayUI.cpp (drawing).

enum RacingRunState {
  RACING_COUNTDOWN,   // "Ready"/"Set"/"Go" intro - first entry or a full restart. Nothing moves yet.
  RACING_RUNNING,
  RACING_PAUSED,       // player-initiated pause during RACING_RUNNING - press resumes, same as Snake's SNAKE_PAUSED
  // A collision just happened - single life, no misses (this task's
  // explicit call: "collision means game over rather than allowing three
  // miss"). Physics frozen, DisplayUI.cpp blinks a "GAME OVER" box over it
  // for GAME_OVER_FLASH_DURATION_MS (Config.h, shared with every other
  // game), then stepRacingGame() advances to RACING_OVER once that elapses.
  RACING_OVER_FLASH,
  RACING_OVER          // resting score card - press to retry
};

struct RacingCar {
  bool active;
  int row;   // 0 (just spawned) .. RACING_PLAYER_ROW (reaching the player)
  int lane;  // 0 .. RACING_LANES-1
};

// Read-only snapshot for DisplayUI's renderer - plain fields rather than
// per-value getters since every field is needed together, every frame.
// Same reasoning as GameSnapshot/SnakeSnapshot/TetrisSnapshot.
struct RacingSnapshot {
  RacingRunState state;
  int countdownValue;       // 2/1/0, meaningful during RACING_COUNTDOWN
  int playerLane;
  RacingCar cars[RACING_MAX_OBSTACLES];
  int score;
  int bestScore;
};

// Resets to a fresh run: RACING_COUNTDOWN state, player centered, all
// obstacles cleared, score 0. Does NOT touch bestScore - persists in RAM
// across runs (and in NVS flash, HighScores.h), same as every other game
// here. Called on entering game mode.
void initRacingGame();

// Alias for initRacingGame() - used when a short press on the game-over
// screen starts a new run. Also replays the Ready/Set/Go countdown.
void racingRestart();

// Shifts the player's lane by +1/-1, clamped to [0, RACING_LANES-1].
// No-op unless RACING_RUNNING - same guard reasoning as gameJump()/
// moleMoveCursor()/tetrisMovePiece().
void racingMoveLane(int direction);

// Toggles RACING_RUNNING <-> RACING_PAUSED. No-op in every other state -
// nothing to pause during a countdown/flash/game-over screen. Mirrors
// Snake's snakeTogglePause() - the original spec's "press reserved for
// pause, for consistency with the other games' conventions."
void racingTogglePause();

// Advances physics by however much real time has elapsed since the last
// call (millis()-based), internally gated to a fixed-cadence row-step tick
// while RACING_RUNNING (see currentTickIntervalMs() in RacingEngine.cpp).
// Caller (DisplayUI's updateRacingFrame()) just needs to call this at
// least once per GAME_FRAME_INTERVAL_MS.
void stepRacingGame();

// True only once the RACING_OVER_FLASH blink has finished and the resting
// score card is showing - i.e. exactly when a short press should restart.
bool isRacingGameOver();

const RacingSnapshot& getRacingSnapshot();
