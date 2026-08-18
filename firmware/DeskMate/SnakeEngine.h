#pragma once

#include <stdint.h>
#include "Config.h"  // SNAKE_GRID_COLS/SNAKE_GRID_ROWS - needed here (not just the .cpp) since SNAKE_MAX_LENGTH sizes the segments[] array below

// Physics/state for the hidden snake game mode. Deliberately has no
// display code in here - same split as GameEngine.h (Dino) and
// WhackEngine.h (Whack-a-mole) vs DisplayUI.cpp (drawing).

enum SnakeRunState {
  SNAKE_COUNTDOWN,    // "Ready"/"Set"/"Go" intro - first entry or a full
                       // restart from SNAKE_OVER only. Nothing moves yet.
  SNAKE_RUNNING,
  // Non-fatal self-hit just happened (lives remain after the decrement) -
  // world frozen exactly as it looked at the moment of the hit.
  // DisplayUI.cpp blinks a "HIT!" box over it for
  // SNAKE_HIT_FLASH_DURATION_MS (Config.h), then stepSnakeGame() itself
  // retreats the snake and advances to SNAKE_RECOUNTDOWN once that elapses.
  SNAKE_HIT_FLASH,
  // 3/2/1 countdown after a non-fatal hit - numbers only, no "READY/SET/GO"
  // words (see countdownShowWords in SnakeSnapshot). The snake has already
  // been retreated by the time this state is entered.
  SNAKE_RECOUNTDOWN,
  // Last life just lost - same shape as GameEngine.h's GAME_OVER_FLASH:
  // physics frozen, DisplayUI.cpp blinks a "GAME OVER" box over it for
  // GAME_OVER_FLASH_DURATION_MS, then stepSnakeGame() advances to
  // SNAKE_OVER once that elapses.
  SNAKE_OVER_FLASH,
  SNAKE_OVER,          // resting score card - press to retry
  SNAKE_PAUSED         // player-initiated pause during SNAKE_RUNNING - press resumes
};

enum SnakeDirection { SNAKE_UP = 0, SNAKE_RIGHT, SNAKE_DOWN, SNAKE_LEFT };

struct SnakeCell {
  int8_t x;  // grid column, 0..SNAKE_GRID_COLS-1
  int8_t y;  // grid row, 0..SNAKE_GRID_ROWS-1
};

// Grid is small enough (16x7 = 112 cells, Config.h) that a fixed-size array
// sized to the whole grid is cheap and simplest - length can never exceed
// cell count anyway (no room left to place food beyond that).
#define SNAKE_MAX_LENGTH (SNAKE_GRID_COLS * SNAKE_GRID_ROWS)

// Read-only snapshot for DisplayUI's renderer - plain fields rather than
// per-value getters since every field is needed together, every frame.
// Same reasoning as GameSnapshot/WhackSnapshot.
struct SnakeSnapshot {
  SnakeRunState state;
  int countdownValue;          // 2/1/0, meaningful during SNAKE_COUNTDOWN or SNAKE_RECOUNTDOWN
  bool countdownShowWords;     // true = "READY/SET/GO" (first entry/restart), false = numbers only (post-hit recountdown)
  SnakeCell segments[SNAKE_MAX_LENGTH];  // segments[0] is the head
  int length;
  SnakeDirection heading;      // current facing - DisplayUI needs this to draw the head's eyes on the right edge
  SnakeCell food;
  int score;
  int bestScore;
  int lives;
};

// Resets to a fresh run: SNAKE_COUNTDOWN state, snake back to
// SNAKE_START_LENGTH centered on the board heading SNAKE_RIGHT, fresh
// food, score 0, lives back to GAME_LIVES. Does NOT touch bestScore -
// persists in RAM across runs (and in NVS flash, HighScores.h) same as
// GameEngine.cpp/WhackEngine.cpp. Called on entering game mode.
void initSnakeGame();

// Alias for initSnakeGame() - used when a short press on the game-over
// screen starts a new run. Also replays the full Ready/Set/Go countdown.
void snakeRestart();

// Queues a relative turn for the next tick: +1 = turn right (relative to
// current heading), -1 = turn left. No-op unless SNAKE_RUNNING - same
// guard reasoning as gameJump()/moleMoveCursor(). At most one queued turn
// is kept (a second rotation before the next tick overwrites the first,
// doesn't queue both) so two quick turns can't sneak in an effective
// reversal between ticks.
void snakeQueueTurn(int direction);

// Toggles SNAKE_RUNNING <-> SNAKE_PAUSED. No-op in every other state -
// nothing to pause during a countdown/flash/game-over screen.
void snakeTogglePause();

// Advances physics by however much real time has elapsed since the last
// call (millis()-based, same reasoning as GameEngine.cpp's stepGame()),
// internally gated to a fixed SNAKE_TICK_INTERVAL_MS cadence while
// SNAKE_RUNNING so movement stays one-grid-cell-per-tick regardless of how
// often the caller invokes this. Caller (DisplayUI's updateSnakeFrame())
// just needs to call this at least once per GAME_FRAME_INTERVAL_MS.
void stepSnakeGame();

// True only once the SNAKE_OVER_FLASH blink has finished and the resting
// score card is showing - i.e. exactly when a short press should restart.
bool isSnakeGameOver();

const SnakeSnapshot& getSnakeSnapshot();
