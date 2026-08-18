#include <Arduino.h>
#include "TetrisEngine.h"
#include "Config.h"
#include "HighScores.h"

// NVS key for this game's persisted best score (HighScores.h) - <=15
// chars, unique per game.
#define HIGHSCORE_KEY "tetris"

// Full-row bitmask for TETRIS_COLS bits set - a row is complete when its
// mask equals this.
#define TETRIS_FULL_ROW_MASK  ((uint16_t)((1u << TETRIS_COLS) - 1))

// Score awarded per lock, indexed by how many lines cleared simultaneously
// (0-4) - a small multi-line bonus, not full classic Tetris scoring (out of
// scope for a hobby build, see plan doc section 5).
static const int LINE_CLEAR_POINTS[5] = { 0, 10, 30, 50, 80 };

// Every piece's rotation-0 shape, as 4 (row,col) offsets within a 4x4 box -
// the well-known tetromino layouts. Every other rotation state is derived
// from this at runtime (see rotateCW4x4Cell()/getPieceCells() below) rather
// than hand-authored per state, so there's exactly one shape to get right
// per piece.
struct Offset { int8_t row, col; };
// SINGLE (the 1x1 block) uses 4 *identical* offsets rather than generalizing
// the engine to variable cell counts - every place that consumes a piece's
// shape (collision checks, locking, drawing, the snapshot's pieceCells[4])
// already hard-assumes exactly 4 cells, and 4 copies of the same cell
// satisfies that for free: pieceFits() redundantly checks the same cell 4
// times (harmless), lockPieceAndContinue() ORs the same bit into rowMask 4
// times (harmless, OR is idempotent), drawTetrisBoard() draws the same
// square 4 times (harmless, idempotent draw). Simplest correct option given
// the fixed-4-cells architecture, not a hack.
static const Offset PIECE_BASE[TETRIS_PIECE_COUNT][4] = {
  /* I      */ { {1,0}, {1,1}, {1,2}, {1,3} },
  /* O      */ { {1,1}, {1,2}, {2,1}, {2,2} },
  /* T      */ { {1,0}, {1,1}, {1,2}, {2,1} },
  /* S      */ { {1,1}, {1,2}, {2,0}, {2,1} },
  /* Z      */ { {1,0}, {1,1}, {2,1}, {2,2} },
  /* J      */ { {1,0}, {2,0}, {2,1}, {2,2} },
  /* L      */ { {1,2}, {2,0}, {2,1}, {2,2} },
  /* SINGLE */ { {1,1}, {1,1}, {1,1}, {1,1} },
};

// 90-degree clockwise rotation of a cell within a 4x4 box: (r,c) -> (c, 3-r).
// This is plain box rotation, not SRS with per-state kick tables - matches
// the spec's explicit allowance to simplify rotation handling (see plan doc
// section 1). Applied `state` times (0-3) to the base shape to get any of
// the four orientations.
static Offset rotateCW4x4(Offset o) {
  Offset result;
  result.row = o.col;
  result.col = 3 - o.row;
  return result;
}

// Resolves `type`'s shape at `rotState` (0-3) into 4 offsets, still
// relative to the piece's 4x4 box (not yet placed on the board - callers
// add anchorRow/anchorCol themselves, see cellsAt() below).
static void getPieceOffsets(TetrisPieceType type, int rotState, Offset outOffsets[4]) {
  for (int i = 0; i < 4; i++) {
    Offset o = PIECE_BASE[type][i];
    for (int r = 0; r < rotState; r++) o = rotateCW4x4(o);
    outOffsets[i] = o;
  }
}

static TetrisRunState runState = TETRIS_COUNTDOWN;
static unsigned long countdownStartMs = 0;
static int countdownValue = 2;

static uint16_t rowMask[TETRIS_ROWS];

static TetrisPieceType pieceType = TETRIS_I;
static int pieceRot = 0;
static int anchorRow = 0;
static int anchorCol = 0;
static bool pieceCrossedMid = false;  // this piece's own fast-tick flag - reset on every spawn, see plan doc section 4

static int score = 0;
// Deliberately NOT reset by resetCommon() - persists across runs/restarts
// in RAM, and in NVS flash (HighScores.h), same as every other game's
// bestScore.
static int bestScore = 0;
static bool bestScoreLoaded = false;
static int bestScoreAtRunStart = 0;
static int linesCleared = 0;

static unsigned long gameOverFlashStartMs = 0;
static unsigned long lastTickMs = 0;

static int spawnCol() { return (TETRIS_COLS - 4) / 2; }

// Resolves the currently-falling piece's 4 cells to absolute board
// coordinates - the anchor plus each rotated offset.
static void cellsAt(TetrisPieceType type, int rot, int aRow, int aCol, TetrisCell outCells[4]) {
  Offset offs[4];
  getPieceOffsets(type, rot, offs);
  for (int i = 0; i < 4; i++) {
    outCells[i].row = (int8_t)(aRow + offs[i].row);
    outCells[i].col = (int8_t)(aCol + offs[i].col);
  }
}

static bool cellBlocked(int row, int col) {
  if (col < 0 || col >= TETRIS_COLS) return true;
  if (row < 0 || row >= TETRIS_ROWS) return true;
  return (rowMask[row] & (1u << col)) != 0;
}

// True if this placement is entirely in-bounds and clear of locked cells -
// used for every attempted move (gravity tick, encoder shift, rotate).
static bool pieceFits(TetrisPieceType type, int rot, int aRow, int aCol) {
  TetrisCell cells[4];
  cellsAt(type, rot, aRow, aCol, cells);
  for (int i = 0; i < 4; i++) {
    if (cellBlocked(cells[i].row, cells[i].col)) return false;
  }
  return true;
}

// Highest row (furthest along the fall axis) any of the piece's cells
// currently occupies - what TETRIS_MID_ROW is compared against.
static int pieceMaxRow() {
  TetrisCell cells[4];
  cellsAt(pieceType, pieceRot, anchorRow, anchorCol, cells);
  int maxRow = cells[0].row;
  for (int i = 1; i < 4; i++) {
    if (cells[i].row > maxRow) maxRow = cells[i].row;
  }
  return maxRow;
}

// Recomputed fresh off how far the current piece has fallen rather than
// stored, same approach as SnakeEngine.cpp's currentTickIntervalMs() -
// once crossed, stays fast for the rest of this piece's fall (see
// pieceCrossedMid).
static unsigned long currentTickIntervalMs() {
  if (pieceCrossedMid || pieceMaxRow() >= TETRIS_MID_ROW) {
    pieceCrossedMid = true;
    return TETRIS_TICK_FAST_MS;
  }
  return TETRIS_TICK_INTERVAL_MS;
}

// Picks a new random piece and drops it at the spawn position. Does NOT
// check whether it fits - stepTetrisGame() checks that right after calling
// this and ends the run if not (a blocked spawn IS the game-over
// condition, see the spec's "Game ends when a new piece can't spawn").
static void spawnPiece() {
  pieceType = (TetrisPieceType)random(0, TETRIS_PIECE_COUNT);
  pieceRot = 0;
  anchorRow = 0;
  anchorCol = spawnCol();
  pieceCrossedMid = false;
}

static void resetCommon() {
  if (!bestScoreLoaded) {
    bestScoreLoaded = true;
    bestScore = loadHighScore(HIGHSCORE_KEY);
  }
  bestScoreAtRunStart = bestScore;

  runState = TETRIS_COUNTDOWN;
  countdownStartMs = millis();
  countdownValue = 2;

  for (int r = 0; r < TETRIS_ROWS; r++) rowMask[r] = 0;

  spawnPiece();

  score = 0;
  linesCleared = 0;
  // bestScore intentionally untouched.

  gameOverFlashStartMs = 0;
  lastTickMs = 0;  // next stepTetrisGame() call ticks fresh, not a huge catch-up jump
}

void initTetrisGame() { resetCommon(); }
void tetrisRestart() { resetCommon(); }

// Both movers below reset lastTickMs on a *successful* move - real Tetris'
// "lock delay resets on input" behavior: it pushes the next gravity tick a
// full interval into the future, so a piece resting on the stack (row+1
// blocked) that the player then nudges sideways or rotates gets a fresh
// window before stepTetrisGame() re-checks whether it can fall - i.e. the
// fall genuinely pauses while the player is actively maneuvering, then
// resumes on its own once they stop. No move-count cap on this (real
// guideline Tetris caps it to stop infinite-spin cheese) - not worth the
// complexity here given the input is a rotary encoder, not a keyboard
// someone can spam.
void tetrisMovePiece(int direction) {
  if (runState != TETRIS_RUNNING) return;
  int newCol = anchorCol + (direction > 0 ? 1 : -1);
  if (pieceFits(pieceType, pieceRot, anchorRow, newCol)) {
    anchorCol = newCol;
    lastTickMs = millis();
  }
  // else: blocked by a wall or a locked cell - rejected outright, no partial move.
}

void tetrisRotatePiece() {
  if (runState != TETRIS_RUNNING) return;
  // A single square has no meaningful rotation states - without this, the
  // box-rotation formula (rotateCW4x4()) would still "rotate" it by moving
  // its one occupied cell to a different offset within its 4x4 box each
  // press (there's no integer cell that's a true fixed point of that
  // formula), which would look like the block hopping sideways rather than
  // rotating in place. Explicit no-op instead, since that's what a 1x1
  // rotate actually should look like.
  if (pieceType == TETRIS_SINGLE) return;
  int newRot = (pieceRot + 1) % 4;
  if (pieceFits(pieceType, newRot, anchorRow, anchorCol)) {
    pieceRot = newRot;
    lastTickMs = millis();
  }
  // else: rotated shape would collide - rejected outright, no wall-kick
  // attempts (see plan doc section 1).
}

bool isTetrisGameOver() { return runState == TETRIS_OVER; }

// Locks the current piece into rowMask, clears any completed lines, and
// spawns the next piece (ending the run if it doesn't fit).
static void lockPieceAndContinue() {
  TetrisCell cells[4];
  cellsAt(pieceType, pieceRot, anchorRow, anchorCol, cells);
  for (int i = 0; i < 4; i++) {
    rowMask[cells[i].row] |= (uint16_t)(1u << cells[i].col);
  }

  // Single backward pass: copy non-full rows into a fresh array from the
  // floor end backward, leaving the spawn end zero-filled - handles
  // clearing multiple non-contiguous rows in one pass, no separate
  // "shift everything above down" loop per line (see plan doc section 2).
  uint16_t newMask[TETRIS_ROWS];
  for (int r = 0; r < TETRIS_ROWS; r++) newMask[r] = 0;
  int writeRow = TETRIS_ROWS - 1;
  int clearedThisLock = 0;
  for (int r = TETRIS_ROWS - 1; r >= 0; r--) {
    if (rowMask[r] == TETRIS_FULL_ROW_MASK) {
      clearedThisLock++;
      continue;
    }
    newMask[writeRow--] = rowMask[r];
  }
  for (int r = 0; r < TETRIS_ROWS; r++) rowMask[r] = newMask[r];

  if (clearedThisLock > 0) {
    linesCleared += clearedThisLock;
    score += LINE_CLEAR_POINTS[clearedThisLock];
    if (score > bestScore) bestScore = score;
  }

  spawnPiece();
  if (!pieceFits(pieceType, pieceRot, anchorRow, anchorCol)) {
    runState = TETRIS_OVER_FLASH;
    gameOverFlashStartMs = millis();
    if (bestScore > bestScoreAtRunStart) {
      saveHighScore(HIGHSCORE_KEY, bestScore);
    }
  }
}

void stepTetrisGame() {
  unsigned long now = millis();

  if (runState == TETRIS_OVER) return;  // frozen until tetrisRestart()

  if (runState == TETRIS_OVER_FLASH) {
    // Board stays exactly as it looked at the moment of topout - no
    // physics update here, just watch the clock for the handoff to the
    // resting TETRIS_OVER score card.
    if (now - gameOverFlashStartMs >= GAME_OVER_FLASH_DURATION_MS) {
      runState = TETRIS_OVER;
    }
    return;
  }

  if (runState == TETRIS_COUNTDOWN) {
    unsigned long elapsed = now - countdownStartMs;
    int phase = (int)(elapsed / GAME_COUNTDOWN_PHASE_MS);
    if (phase > 2) phase = 2;
    countdownValue = 2 - phase;

    if (elapsed >= GAME_COUNTDOWN_PHASE_MS * 3) {
      runState = TETRIS_RUNNING;
      lastTickMs = now;
    }
    return;
  }

  // TETRIS_RUNNING from here down - fixed-cadence grid-step tick (one row
  // per tick), not a per-frame fall. Pace depends on how far the current
  // piece has fallen - see currentTickIntervalMs().
  if (now - lastTickMs < currentTickIntervalMs()) return;
  lastTickMs = now;

  if (pieceFits(pieceType, pieceRot, anchorRow + 1, anchorCol)) {
    anchorRow++;
  } else {
    lockPieceAndContinue();
  }
}

const TetrisSnapshot& getTetrisSnapshot() {
  static TetrisSnapshot snap;
  snap.state = runState;
  snap.countdownValue = countdownValue;
  for (int r = 0; r < TETRIS_ROWS; r++) snap.rowMask[r] = rowMask[r];

  // Only meaningful while actually running - the "piece" sitting in
  // pieceType/anchorRow/anchorCol during TETRIS_OVER_FLASH is the blocked
  // spawn attempt that ended the run, never actually placed, so it isn't
  // drawn (the frozen board on its own is the correct "world at the
  // moment of topout" picture).
  snap.pieceActive = (runState == TETRIS_RUNNING);
  if (snap.pieceActive) {
    cellsAt(pieceType, pieceRot, anchorRow, anchorCol, snap.pieceCells);
  }

  snap.score = score;
  snap.bestScore = bestScore;
  snap.linesCleared = linesCleared;
  return snap;
}
