#include <Arduino.h>
#include "TicTacToeEngine.h"
#include "Config.h"

static TTTRunState runState = TTT_RUNNING;
static TTTSelectMode selectMode = TTT_SELECT_BOARD;
static int activeBoard = -1;
static int cursorIndex = 0;
static TTTMark currentPlayer = TTT_X;

static TTTMark cellMark[9][9];
static TTTMark subOwner[9];
static TTTMark overallWinner = TTT_EMPTY;

static unsigned long gameOverFlashStartMs = 0;

// The 8 standard 3-in-a-row lines over a flat 9-cell row-major grid - reused
// as-is for both a sub-board's own 9 cells and the 9 sub-board owners
// (meta-board), since both are "a 3x3 grid of 9 row-major slots" at heart.
static const int WIN_LINES[8][3] = {
  {0, 1, 2}, {3, 4, 5}, {6, 7, 8},
  {0, 3, 6}, {1, 4, 7}, {2, 5, 8},
  {0, 4, 8}, {2, 4, 6},
};

// Returns TTT_X or TTT_O if one of the 8 lines is completed by that mark,
// else TTT_EMPTY. TTT_DRAW never matches a line (a drawn sub-board doesn't
// count toward either player's meta-line) and TTT_EMPTY obviously doesn't
// either, so checking arr[a]==arr[b]==arr[c] plus "is it actually X or O"
// handles both grids with the same logic.
static TTTMark checkLine9(const TTTMark arr[9]) {
  for (int i = 0; i < 8; i++) {
    int a = WIN_LINES[i][0], b = WIN_LINES[i][1], c = WIN_LINES[i][2];
    if (arr[a] != TTT_EMPTY && arr[a] != TTT_DRAW && arr[a] == arr[b] && arr[b] == arr[c]) {
      return arr[a];
    }
  }
  return TTT_EMPTY;
}

static bool boardFull9(const TTTMark arr[9]) {
  for (int i = 0; i < 9; i++) {
    if (arr[i] == TTT_EMPTY) return false;
  }
  return true;
}

static bool subBoardOpen(int board) {
  return subOwner[board] == TTT_EMPTY;
}

static bool cellOpen(int board, int cell) {
  return cellMark[board][cell] == TTT_EMPTY;
}

// First open cell within a sub-board, row-major - only ever called on a
// sub-board known to still have at least one, since callers only route
// into an open board in the first place.
static int firstOpenCell(int board) {
  for (int i = 0; i < 9; i++) {
    if (cellOpen(board, i)) return i;
  }
  return 0;
}

// First still-open sub-board, row-major - only ever called when at least
// one exists (the game would already be TTT_OVER via the tie check
// otherwise).
static int firstOpenSubBoard() {
  for (int i = 0; i < 9; i++) {
    if (subBoardOpen(i)) return i;
  }
  return 0;
}

static void resetCommon() {
  runState = TTT_RUNNING;
  selectMode = TTT_SELECT_BOARD;
  activeBoard = -1;
  cursorIndex = 0;
  currentPlayer = TTT_X;  // X always opens - simplest fixed default, no coin flip needed for a local 2-player game
  overallWinner = TTT_EMPTY;

  for (int b = 0; b < 9; b++) {
    subOwner[b] = TTT_EMPTY;
    for (int c = 0; c < 9; c++) {
      cellMark[b][c] = TTT_EMPTY;
    }
  }

  gameOverFlashStartMs = 0;
}

void initTicTacToeGame() {
  resetCommon();
}

void ticTacToeRestart() {
  resetCommon();
}

void ticTacToeMoveCursor(int direction) {
  if (runState != TTT_RUNNING) return;

  for (int i = 0; i < 9; i++) {
    cursorIndex = (cursorIndex + direction + 9) % 9;
    if (selectMode == TTT_SELECT_BOARD) {
      if (subBoardOpen(cursorIndex)) return;
    } else {
      if (cellOpen(activeBoard, cursorIndex)) return;
    }
  }
  // No legal target found in 9 tries - can't actually happen (would mean
  // the current grid has zero open slots, which the win/tie check below
  // would already have ended the run over before offering a turn on it).
}

void ticTacToePress() {
  if (runState != TTT_RUNNING) return;

  if (selectMode == TTT_SELECT_BOARD) {
    // Lock in the sub-board - no mark placed yet, just moves to picking a
    // cell within it. Cursor starts on that sub-board's first open cell.
    activeBoard = cursorIndex;
    selectMode = TTT_SELECT_CELL;
    cursorIndex = firstOpenCell(activeBoard);
    return;
  }

  // TTT_SELECT_CELL: place the current player's mark.
  int placedCell = cursorIndex;
  cellMark[activeBoard][placedCell] = currentPlayer;

  TTTMark subLine = checkLine9(cellMark[activeBoard]);
  if (subLine != TTT_EMPTY) {
    subOwner[activeBoard] = subLine;
  } else if (boardFull9(cellMark[activeBoard])) {
    subOwner[activeBoard] = TTT_DRAW;
  }

  if (subOwner[activeBoard] != TTT_EMPTY) {
    TTTMark metaLine = checkLine9(subOwner);
    if (metaLine != TTT_EMPTY) {
      overallWinner = metaLine;
      runState = TTT_OVER_FLASH;
      gameOverFlashStartMs = millis();
      return;
    }
    if (boardFull9(subOwner)) {
      overallWinner = TTT_DRAW;
      runState = TTT_OVER_FLASH;
      gameOverFlashStartMs = millis();
      return;
    }
  }

  // Game continues - flip the turn and route it. The cell just played
  // (its position within its own sub-board) is the standard Ultimate
  // Tic-Tac-Toe rule for which sub-board the other player is sent to next.
  currentPlayer = (currentPlayer == TTT_X) ? TTT_O : TTT_X;
  int forcedBoard = placedCell;
  if (subBoardOpen(forcedBoard)) {
    activeBoard = forcedBoard;
    selectMode = TTT_SELECT_CELL;
    cursorIndex = firstOpenCell(activeBoard);
  } else {
    // Forced board is already decided - a "free move", same free pick as
    // the very first move of the game.
    activeBoard = -1;
    selectMode = TTT_SELECT_BOARD;
    cursorIndex = firstOpenSubBoard();
  }
}

void stepTicTacToeGame() {
  if (runState != TTT_OVER_FLASH) return;
  if (millis() - gameOverFlashStartMs >= GAME_OVER_FLASH_DURATION_MS) {
    runState = TTT_OVER;
  }
}

bool isTicTacToeGameOver() {
  return runState == TTT_OVER;
}

const TTTSnapshot& getTicTacToeSnapshot() {
  static TTTSnapshot snap;
  snap.state = runState;
  snap.selectMode = selectMode;
  snap.activeBoard = activeBoard;
  snap.cursorIndex = cursorIndex;
  snap.currentPlayer = currentPlayer;
  for (int b = 0; b < 9; b++) {
    snap.subOwner[b] = subOwner[b];
    for (int c = 0; c < 9; c++) {
      snap.cellMark[b][c] = cellMark[b][c];
    }
  }
  snap.overallWinner = overallWinner;
  return snap;
}
