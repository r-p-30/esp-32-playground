#include <Arduino.h>
#include "SnakeEngine.h"
#include "Config.h"
#include "HighScores.h"

// NVS key for this game's persisted best score - <=15 chars, unique per game.
#define HIGHSCORE_KEY "snake"

static SnakeRunState runState = SNAKE_COUNTDOWN;
static unsigned long countdownStartMs = 0;
static int countdownValue = 2;
static bool countdownShowWords = true;

static SnakeCell segments[SNAKE_MAX_LENGTH];
static int length = SNAKE_START_LENGTH;
static SnakeDirection heading = SNAKE_RIGHT;
static int queuedTurn = 0;                        // -1/0/+1, consumed on the next tick
static SnakeDirection headingAtHit = SNAKE_RIGHT;  // direction actually walked into the crash - the plain-nudge retreat's backward vector

// Snapshot of the snake exactly as it looked right before the most recent
// turn, plus how many SNAKE_RUNNING ticks have elapsed since (0 = the turn
// was taken this tick). If a hit lands within SNAKE_TURN_HIT_WINDOW_TICKS
// of that turn, retreatSnake() rewinds all the way back to this snapshot
// instead of just nudging back a couple of cells - see stepSnakeGame()'s
// hit-handling block. Re-captured on every subsequent turn, so it always
// reflects the *most recent* one, not the first one this run.
static SnakeCell preTurnSegments[SNAKE_MAX_LENGTH];
static int preTurnLength = SNAKE_START_LENGTH;
static SnakeDirection preTurnHeading = SNAKE_RIGHT;
static int ticksSinceLastTurn = 0;

// Which of the two retreat styles the next retreatSnake() call should use -
// decided at hit time (stepSnakeGame()), applied once the HIT_FLASH beat
// elapses (retreatSnake() itself).
static bool rewindToPreTurn = false;
static SnakeDirection headingToResume = SNAKE_RIGHT;

static SnakeCell food;

static int lives = GAME_LIVES;
static unsigned long hitFlashStartMs = 0;
static unsigned long gameOverFlashStartMs = 0;

static unsigned long lastTickMs = 0;

static int score = 0;
// Deliberately NOT reset by resetCommon() - persists across runs/restarts
// in RAM, and in NVS flash (HighScores.h), same as GameEngine.cpp/
// WhackEngine.cpp's bestScore.
static int bestScore = 0;
static bool bestScoreLoaded = false;
static int bestScoreAtRunStart = 0;

// dx/dy per heading, indexed by SnakeDirection - UP/RIGHT/DOWN/LEFT is a
// clockwise ordering, so +1 = clockwise = "turn right", -1 = "turn left".
static const int8_t DX[4] = { 0, 1, 0, -1 };
static const int8_t DY[4] = { -1, 0, 1, 0 };

static int8_t wrapCol(int v) { return (int8_t)((v + SNAKE_GRID_COLS) % SNAKE_GRID_COLS); }
static int8_t wrapRow(int v) { return (int8_t)((v + SNAKE_GRID_ROWS) % SNAKE_GRID_ROWS); }

static void placeFood() {
  bool onSnake;
  do {
    food.x = (int8_t)random(0, SNAKE_GRID_COLS);
    food.y = (int8_t)random(0, SNAKE_GRID_ROWS);
    onSnake = false;
    for (int i = 0; i < length; i++) {
      if (segments[i].x == food.x && segments[i].y == food.y) { onSnake = true; break; }
    }
  } while (onSnake);
}

// Ramps down every SNAKE_SPEEDUP_STEP_SCORE points, floored at
// SNAKE_TICK_MIN_MS - recomputed fresh off current score rather than
// stored, same approach as WhackEngine.cpp's currentMoleVisibleMs().
static unsigned long currentTickIntervalMs() {
  int steps = score / SNAKE_SPEEDUP_STEP_SCORE;
  long ms = (long)SNAKE_TICK_INTERVAL_MS - steps * (long)SNAKE_SPEEDUP_STEP_MS;
  if (ms < (long)SNAKE_TICK_MIN_MS) ms = (long)SNAKE_TICK_MIN_MS;
  return (unsigned long)ms;
}

static void resetCommon() {
  if (!bestScoreLoaded) {
    bestScoreLoaded = true;
    bestScore = loadHighScore(HIGHSCORE_KEY);
  }
  bestScoreAtRunStart = bestScore;

  runState = SNAKE_COUNTDOWN;
  countdownStartMs = millis();
  countdownValue = 2;
  countdownShowWords = true;

  length = SNAKE_START_LENGTH;
  heading = SNAKE_RIGHT;
  queuedTurn = 0;
  int startCol = SNAKE_GRID_COLS / 2;
  int startRow = SNAKE_GRID_ROWS / 2;
  for (int i = 0; i < length; i++) {
    segments[i].x = wrapCol(startCol - i);  // head at [0], body trails left of it (opposite of SNAKE_RIGHT heading)
    segments[i].y = (int8_t)startRow;
  }

  for (int i = 0; i < length; i++) preTurnSegments[i] = segments[i];
  preTurnLength = length;
  preTurnHeading = heading;
  // Sentinel "no turn taken yet" value, safely outside the window so an
  // early hit (before the player has turned at all) takes the plain-nudge
  // path, not a rewind-to-nonexistent-turn path.
  ticksSinceLastTurn = SNAKE_TURN_HIT_WINDOW_TICKS + 1;

  placeFood();

  lives = GAME_LIVES;
  hitFlashStartMs = 0;
  gameOverFlashStartMs = 0;

  lastTickMs = 0;  // next stepSnakeGame() call ticks fresh, not a huge catch-up jump

  score = 0;
  // bestScore intentionally untouched.
}

void initSnakeGame() { resetCommon(); }
void snakeRestart() { resetCommon(); }

void snakeQueueTurn(int direction) {
  if (runState != SNAKE_RUNNING) return;
  queuedTurn = (direction > 0) ? 1 : -1;
}

void snakeTogglePause() {
  if (runState == SNAKE_RUNNING) {
    runState = SNAKE_PAUSED;
  } else if (runState == SNAKE_PAUSED) {
    runState = SNAKE_RUNNING;
    lastTickMs = millis();  // resume waits one fresh interval, doesn't move instantly on the same frame it un-pauses
  }
}

bool isSnakeGameOver() { return runState == SNAKE_OVER; }

// Applies queuedTurn to heading and consumes it. SnakeDirection's
// UP/RIGHT/DOWN/LEFT ordering is clockwise, so adding queuedTurn (+1/-1)
// mod 4 is exactly "turn right/left relative to current heading." Snapshots
// the pre-turn state first - see preTurnSegments's comment above for why.
static void applyQueuedTurn() {
  if (queuedTurn != 0) {
    for (int i = 0; i < length; i++) preTurnSegments[i] = segments[i];
    preTurnLength = length;
    preTurnHeading = heading;
    ticksSinceLastTurn = 0;

    heading = (SnakeDirection)((heading + queuedTurn + 4) % 4);
    queuedTurn = 0;
  }
}

// Undoes the crash, one of two ways depending on what stepSnakeGame()
// decided at hit time (rewindToPreTurn):
//  - Recent turn (within SNAKE_TURN_HIT_WINDOW_TICKS): the turn itself is
//    what's responsible, so rewind position *and* heading all the way back
//    to exactly how the snake looked right before that turn - a plain
//    cell-nudge wouldn't fix anything here, since re-walking the same
//    (still-fatal) turned direction would just retrace right back into it.
//  - No recent turn: nudge the whole snake back SNAKE_HIT_RETREAT_CELLS
//    cells, opposite whichever direction it was heading at the moment of
//    the hit, heading unchanged. Translating the *entire* body uniformly
//    (not just the head, not shortening it) is what makes this safe: a
//    shape with no self-overlap stays with no self-overlap under a uniform
//    shift, so this can never manufacture a new collision - it only ever
//    moves the existing shape clear of the one that just happened.
// See docs/game-mode-plan.md's Snake section for the full reasoning.
static void retreatSnake() {
  if (rewindToPreTurn) {
    for (int i = 0; i < preTurnLength; i++) segments[i] = preTurnSegments[i];
    length = preTurnLength;
    // Any food eaten during the rewound ticks stays eaten (score already
    // banked, same as every other hit not touching score) even though the
    // segments it grew are gone again - a harmless cosmetic mismatch
    // between score and on-screen length, not a crash or invalid state.
  } else {
    int8_t bx = (int8_t)(-DX[headingAtHit] * SNAKE_HIT_RETREAT_CELLS);
    int8_t by = (int8_t)(-DY[headingAtHit] * SNAKE_HIT_RETREAT_CELLS);
    for (int i = 0; i < length; i++) {
      segments[i].x = wrapCol(segments[i].x + bx);
      segments[i].y = wrapRow(segments[i].y + by);
    }
  }
  heading = headingToResume;
  queuedTurn = 0;  // discard anything queued right before the hit
  // Back to a clean "no turn taken yet" slate - the turn that caused this
  // (if any) has now been fully undone, not just worked around.
  ticksSinceLastTurn = SNAKE_TURN_HIT_WINDOW_TICKS + 1;
}

static void beginRecountdown() {
  runState = SNAKE_RECOUNTDOWN;
  countdownStartMs = millis();
  countdownValue = 2;
  countdownShowWords = false;
}

void stepSnakeGame() {
  unsigned long now = millis();

  if (runState == SNAKE_OVER || runState == SNAKE_PAUSED) return;  // frozen until snakeRestart()/snakeTogglePause()

  if (runState == SNAKE_OVER_FLASH) {
    if (now - gameOverFlashStartMs >= GAME_OVER_FLASH_DURATION_MS) {
      runState = SNAKE_OVER;
    }
    return;
  }

  if (runState == SNAKE_HIT_FLASH) {
    if (now - hitFlashStartMs >= SNAKE_HIT_FLASH_DURATION_MS) {
      retreatSnake();
      beginRecountdown();
    }
    return;
  }

  if (runState == SNAKE_COUNTDOWN || runState == SNAKE_RECOUNTDOWN) {
    unsigned long elapsed = now - countdownStartMs;
    unsigned long phaseMs = countdownShowWords ? GAME_COUNTDOWN_PHASE_MS : SNAKE_RECOUNTDOWN_PHASE_MS;
    int phase = (int)(elapsed / phaseMs);
    if (phase > 2) phase = 2;
    countdownValue = 2 - phase;

    if (elapsed >= phaseMs * 3) {
      runState = SNAKE_RUNNING;
      lastTickMs = now;
    }
    return;
  }

  // SNAKE_RUNNING from here down - fixed-cadence grid-step tick, not a
  // per-frame move.
  if (now - lastTickMs < currentTickIntervalMs()) return;
  lastTickMs = now;

  // Counts up every running tick; applyQueuedTurn() resets it to 0 on
  // whichever tick actually contains a turn (order doesn't matter - it
  // just gets overwritten that tick).
  ticksSinceLastTurn++;
  applyQueuedTurn();

  SnakeCell nextHead;
  nextHead.x = wrapCol(segments[0].x + DX[heading]);
  nextHead.y = wrapRow(segments[0].y + DY[heading]);

  bool ateFood = (nextHead.x == food.x && nextHead.y == food.y);

  // Self-collision check, with tail forgiveness: a hit on one of the last
  // SNAKE_TAIL_FORGIVENESS_SEGMENTS segments is ignored outright (see
  // Config.h). Checked against segments[1..length-1] - segments[0] is the
  // current head, can't collide with itself.
  bool hit = false;
  for (int i = 1; i < length; i++) {
    if (segments[i].x == nextHead.x && segments[i].y == nextHead.y) {
      if (i >= length - SNAKE_TAIL_FORGIVENESS_SEGMENTS) continue;  // forgiven
      hit = true;
      break;
    }
  }

  if (hit) {
    headingAtHit = heading;  // direction actually walked into the crash - used by the plain-nudge retreat path
    // Was the most recent turn recent enough to be the real cause? See
    // SNAKE_TURN_HIT_WINDOW_TICKS's comment (Config.h) and retreatSnake().
    rewindToPreTurn = (ticksSinceLastTurn <= SNAKE_TURN_HIT_WINDOW_TICKS);
    headingToResume = rewindToPreTurn ? preTurnHeading : heading;
    lives--;
    if (lives <= 0) {
      runState = SNAKE_OVER_FLASH;
      gameOverFlashStartMs = now;
      if (bestScore > bestScoreAtRunStart) {
        saveHighScore(HIGHSCORE_KEY, bestScore);
      }
    } else {
      runState = SNAKE_HIT_FLASH;
      hitFlashStartMs = now;
    }
    return;  // world stays exactly as it was at the moment of the hit
  }

  // Normal move: shift every segment down one, new head at the front.
  if (ateFood) {
    if (length < SNAKE_MAX_LENGTH) {
      for (int i = length; i > 0; i--) segments[i] = segments[i - 1];
      length++;
    }
    // else: board is completely full (112/112 cells) - nowhere left to
    // grow. Unreachable in any real play session, not worth a "you win"
    // screen for a hobby game; just stops growing.
    segments[0] = nextHead;
    score++;
    if (score > bestScore) bestScore = score;
    if (length < SNAKE_MAX_LENGTH) placeFood();  // guards placeFood()'s do-while against spinning forever with zero free cells
  } else {
    for (int i = length - 1; i > 0; i--) segments[i] = segments[i - 1];
    segments[0] = nextHead;
  }
}

const SnakeSnapshot& getSnakeSnapshot() {
  static SnakeSnapshot snap;
  snap.state = runState;
  snap.countdownValue = countdownValue;
  snap.countdownShowWords = countdownShowWords;
  for (int i = 0; i < length; i++) snap.segments[i] = segments[i];
  snap.length = length;
  snap.heading = heading;
  snap.food = food;
  snap.score = score;
  snap.bestScore = bestScore;
  snap.lives = lives;
  return snap;
}
