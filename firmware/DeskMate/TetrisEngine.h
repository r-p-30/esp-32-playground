#pragma once

#include <stdint.h>
#include "Config.h"  // TETRIS_ROWS/TETRIS_COLS - needed here (not just the .cpp) since they size the board below

// Physics/state for the hidden Tetris game mode. Deliberately has no
// display code in here - same split as GameEngine.h/WhackEngine.h/
// SnakeEngine.h vs DisplayUI.cpp (drawing). Board/piece coordinates stay in
// *game* space (row = fall axis, col = shift axis) the whole time - the
// 90-degree screen rotation described in docs/tetris-implementation-plan.md
// is purely a render-time concern, handled entirely in DisplayUI.cpp.

enum TetrisRunState {
  TETRIS_COUNTDOWN,   // "Ready"/"Set"/"Go" intro, nothing falls yet
  TETRIS_RUNNING,
  // Topout just happened - board frozen exactly as it looked at that
  // instant, DisplayUI.cpp blinks a "GAME OVER" box over it for
  // GAME_OVER_FLASH_DURATION_MS (Config.h, shared across every game), then
  // stepTetrisGame() itself advances to TETRIS_OVER once that elapses. Same
  // 4-state shape as GameEngine.h - no lives/non-fatal-hit concept here, a
  // blocked spawn ends the run outright (see stepTetrisGame()).
  TETRIS_OVER_FLASH,
  TETRIS_OVER         // resting score card - press to retry
};

// The 7 standard tetrominoes, plus one deliberate non-standard addition:
// TETRIS_SINGLE, a bare 1x1 block - not part of real Tetris, added on
// request. See TetrisEngine.cpp's PIECE_BASE/tetrisRotatePiece() for how a
// piece type built around the engine's fixed "always 4 cells" assumption
// (TetrisCell pieceCells[4] below, cellsAt() always resolving exactly 4)
// represents something that's visually only 1 cell.
enum TetrisPieceType { TETRIS_I = 0, TETRIS_O, TETRIS_T, TETRIS_S, TETRIS_Z, TETRIS_J, TETRIS_L, TETRIS_SINGLE, TETRIS_PIECE_COUNT };

struct TetrisCell {
  int8_t row;  // 0..TETRIS_ROWS-1, fall axis
  int8_t col;  // 0..TETRIS_COLS-1, shift axis (what the encoder moves)
};

// Read-only snapshot for DisplayUI's renderer - plain fields rather than
// per-value getters since every field is needed together, every frame. Same
// reasoning as GameSnapshot/WhackSnapshot/SnakeSnapshot.
struct TetrisSnapshot {
  TetrisRunState state;
  int countdownValue;  // 2/1/0, only meaningful during TETRIS_COUNTDOWN

  // Locked board - one bit per column, LSB = col 0. uint16_t (not uint8_t)
  // so a future TETRIS_COLS > 8 doesn't silently overflow the mask, even
  // though only the low TETRIS_COLS bits are ever set today.
  uint16_t rowMask[TETRIS_ROWS];

  // Falling piece - four occupied cells, already resolved to absolute
  // board coordinates (anchor + rotated offsets already applied), so
  // DisplayUI doesn't need to know anything about piece shapes/rotation
  // math to draw it.
  TetrisCell pieceCells[4];
  bool pieceActive;  // false during countdown/over - no piece to draw yet

  int score;
  int bestScore;
  // Cumulative lines cleared this run - Games.cpp's wrapper diffs this
  // frame-to-frame (same "delta detection" pattern as SnakeSnapshot.score
  // for "food just eaten") to know how many lines cleared on the most
  // recent lock, so it can pick the right playGameLineClear() tone.
  int linesCleared;
};

// Resets to a fresh run: TETRIS_COUNTDOWN state, empty board, score/
// linesCleared back to 0. Does NOT touch bestScore - persists in RAM
// across runs (and in NVS flash, HighScores.h), same as every other game's
// bestScore. Called on entering game mode.
void initTetrisGame();

// Alias for initTetrisGame() - used when a short press on the game-over
// screen starts a new run. Also replays the Ready/Set/Go countdown.
void tetrisRestart();

// Shifts the falling piece one column in the given direction (+1/-1) - the
// encoder's rotation, mapped straight through same as every other game's
// onRotate(). No-op if TETRIS_RUNNING isn't the current state, or if the
// shifted position would collide with a wall or a locked cell (rejected
// outright, no partial move).
void tetrisMovePiece(int direction);

// Rotates the falling piece 90 degrees (the press action, per this task's
// explicit control scheme - press is fully committed to rotation, nothing
// else is bound to it while TETRIS_RUNNING). No-op if the rotated shape
// would collide - rejected outright, no wall-kick attempts (see plan doc
// section 1's note on why rotation stays this simple).
void tetrisRotatePiece();

// Advances physics by however much real time has elapsed since the last
// call (millis()-based, same reasoning as every other engine's step
// function), internally gated to TETRIS_TICK_INTERVAL_MS or
// TETRIS_TICK_FAST_MS (Config.h) depending on how far the current piece has
// fallen - see docs/tetris-implementation-plan.md section 4. Caller
// (DisplayUI's updateTetrisFrame()) just needs to call this at least once
// per GAME_FRAME_INTERVAL_MS.
void stepTetrisGame();

// True only once the TETRIS_OVER_FLASH blink has finished and the resting
// score card is showing - i.e. exactly when a short press should restart.
bool isTetrisGameOver();

const TetrisSnapshot& getTetrisSnapshot();
