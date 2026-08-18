#pragma once

// Physics/state for Ultimate Tic-Tac-Toe (9x9 grid, a 3x3 meta-grid of 3x3
// sub-boards). Deliberately has no display code in here - same split as
// every other Engine.h in this codebase.
//
// Deviates from the shared Game.h lifecycle contract on purpose: no
// GAME_COUNTDOWN (a turn-based board game has no "get ready" beat to play),
// no lives/misses, and no best score (there's no meaningful "high score"
// for a two-player win/tie game) - see Games.cpp's ticTacToeBestScore()
// always returning 0. It still ends with a GAME_OVER_FLASH beat + the
// shared alarm on a win/tie, and a resting GAME_OVER score card, same as
// every other game.

enum TTTMark {
  TTT_EMPTY = 0,
  TTT_X,
  TTT_O,
  TTT_DRAW,  // only ever a sub-board owner or the overall result, never a raw cell mark
};

enum TTTRunState {
  TTT_RUNNING,
  // Win or tie just happened - board frozen exactly as it looked at that
  // instant, DisplayUI.cpp blinks a "X WINS"/"O WINS"/"TIE GAME" box over
  // it for GAME_OVER_FLASH_DURATION_MS (Config.h), then stepTicTacToeGame()
  // advances to TTT_OVER once that elapses. Same shape as every other
  // game's *_OVER_FLASH.
  TTT_OVER_FLASH,
  TTT_OVER  // resting result card - press to retry
};

// Which grid a rotate/press currently acts on: the 3x3 meta-grid (picking
// a sub-board) or the 3x3 grid of cells within whichever sub-board was just
// picked. A turn always starts in one or the other depending on where the
// previous move sent it - see ticTacToePress()'s forced/free routing.
enum TTTSelectMode {
  TTT_SELECT_BOARD,
  TTT_SELECT_CELL
};

// Read-only snapshot for DisplayUI's renderer - plain fields rather than
// per-value getters since every field is needed together, every frame.
struct TTTSnapshot {
  TTTRunState state;
  TTTSelectMode selectMode;
  int activeBoard;        // 0-8, which sub-board is locked in for TTT_SELECT_CELL; -1 during TTT_SELECT_BOARD
  int cursorIndex;         // 0-8 within whichever grid selectMode currently targets
  TTTMark currentPlayer;    // TTT_X or TTT_O - whose turn it is
  TTTMark cellMark[9][9];    // [subBoard][cellInSubBoard], row-major within each
  TTTMark subOwner[9];        // TTT_EMPTY = still open, else who claimed it (or TTT_DRAW)
  TTTMark overallWinner;       // TTT_EMPTY while still playing, else the final result
};

// Resets to a fresh run: TTT_RUNNING immediately (no countdown), empty
// board, X to move first, turn starts in TTT_SELECT_BOARD since there's no
// forced sub-board yet. Called on entering game mode.
void initTicTacToeGame();

// Alias for initTicTacToeGame() - used when a short press on the result
// screen starts a new run.
void ticTacToeRestart();

// Moves the cursor one legal target in the given direction (+1/-1) within
// whichever grid selectMode currently targets, skipping already-decided
// sub-boards (TTT_SELECT_BOARD) or already-filled cells (TTT_SELECT_CELL)
// and wrapping around - same "browse legal options" idea as the game
// picker menu's own rotate handling. No-op unless TTT_RUNNING.
void ticTacToeMoveCursor(int direction);

// Commits the cursor's current target. In TTT_SELECT_BOARD, locks in that
// sub-board and switches to TTT_SELECT_CELL (cursor lands on that
// sub-board's first open cell) - no mark is placed yet. In TTT_SELECT_CELL,
// places the current player's mark at the cursor's cell, then resolves any
// resulting sub-board/overall win or tie, flips the turn, and routes the
// next turn: forced into whichever sub-board matches the cell just played
// if that sub-board is still open, otherwise back to a free
// TTT_SELECT_BOARD pick (mirrors the very first move's own free pick). A
// press is always fully committed - no undo, no pause. No-op unless
// TTT_RUNNING.
void ticTacToePress();

// Advances TTT_OVER_FLASH to TTT_OVER once GAME_OVER_FLASH_DURATION_MS has
// elapsed - the only thing that ever auto-advances here, since nothing
// else in a turn-based board game moves on its own. Still called every
// loop() same as every other game's step function for consistency.
void stepTicTacToeGame();

// True only once the TTT_OVER_FLASH blink has finished and the resting
// result card is showing - i.e. exactly when a short press should restart.
bool isTicTacToeGameOver();

const TTTSnapshot& getTicTacToeSnapshot();
