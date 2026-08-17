#include <Arduino.h>
#include "WhackEngine.h"
#include "Config.h"

static WhackRunState runState = WHACK_COUNTDOWN;
static unsigned long countdownStartMs = 0;
static int countdownValue = 2;

static int cursorPos = 0;
static int molePos = -1;
static unsigned long moleSpawnMs = 0;

static int score = 0;
// Deliberately NOT reset by resetCommon() - persists across runs/restarts
// until reboot or reflash, same as GameEngine.cpp's bestScore.
static int bestScore = 0;
static int misses = 0;

static unsigned long gameOverFlashStartMs = 0;

// Shrinks the visible window by one MOLE_VISIBLE_MS_STEP per
// MOLE_DIFFICULTY_STEP_SCORE points of score, floored at
// MOLE_VISIBLE_MS_MIN - recomputed fresh each spawn rather than stored, so
// it always reflects the score at that moment.
static unsigned long currentMoleVisibleMs() {
  int steps = score / MOLE_DIFFICULTY_STEP_SCORE;
  long ms = (long)MOLE_VISIBLE_MS - steps * (long)MOLE_VISIBLE_MS_STEP;
  if (ms < (long)MOLE_VISIBLE_MS_MIN) ms = (long)MOLE_VISIBLE_MS_MIN;
  return (unsigned long)ms;
}

// Picks a random hole, excluding excludePos (pass -1 to allow any hole) -
// keeps the mole from reappearing on the same hole it was just hit/missed
// on, same "next spawn differs from last" reasoning as GameEngine.cpp's
// randomSpawnGap() keeps cactus gaps varied.
static int randomMolePos(int excludePos) {
  if (excludePos < 0) return random(0, MOLE_POSITIONS);
  int pos;
  do {
    pos = random(0, MOLE_POSITIONS);
  } while (pos == excludePos);
  return pos;
}

static void resetCommon() {
  runState = WHACK_COUNTDOWN;
  countdownStartMs = millis();
  countdownValue = 2;

  cursorPos = 0;
  molePos = -1;
  moleSpawnMs = 0;

  score = 0;
  misses = 0;
  // bestScore intentionally untouched.

  gameOverFlashStartMs = 0;
}

void initMoleGame() {
  resetCommon();
}

void moleRestart() {
  resetCommon();
}

void moleMoveCursor(int direction) {
  if (runState != WHACK_RUNNING) return;  // no aiming during countdown or after game over
  cursorPos = (cursorPos + direction + MOLE_POSITIONS) % MOLE_POSITIONS;
}

void moleWhack() {
  if (runState != WHACK_RUNNING) return;
  if (cursorPos != molePos) return;  // whiffed press - not a miss, just nothing happens

  score++;
  if (score > bestScore) bestScore = score;
  molePos = randomMolePos(molePos);
  moleSpawnMs = millis();
}

bool isMoleGameOver() {
  return runState == WHACK_OVER;
}

void stepMoleGame() {
  unsigned long now = millis();

  if (runState == WHACK_OVER) return;  // frozen until moleRestart()

  if (runState == WHACK_OVER_FLASH) {
    // World stays exactly as it was at the moment of the last miss - see
    // the WHACK_MAX_MISSES branch below for how molePos gets set to -1
    // right before this state is entered, so the frozen frame shows no
    // mole rather than one mid-timeout.
    if (now - gameOverFlashStartMs >= GAME_OVER_FLASH_DURATION_MS) {
      runState = WHACK_OVER;
    }
    return;
  }

  if (runState == WHACK_COUNTDOWN) {
    unsigned long elapsed = now - countdownStartMs;
    int phase = (int)(elapsed / GAME_COUNTDOWN_PHASE_MS);
    if (phase > 2) phase = 2;
    countdownValue = 2 - phase;

    if (elapsed >= GAME_COUNTDOWN_PHASE_MS * 3) {
      runState = WHACK_RUNNING;
      molePos = randomMolePos(-1);
      moleSpawnMs = now;
    }
    return;
  }

  // WHACK_RUNNING from here down.
  if (now - moleSpawnMs >= currentMoleVisibleMs()) {
    misses++;
    if (misses >= MOLE_MAX_MISSES) {
      // Last allowed miss - dismiss the mole (no respawn) so the frozen
      // WHACK_OVER_FLASH frame shows an empty board, same as GameEngine.cpp
      // dismissing the cactus on a fatal hit before freezing.
      molePos = -1;
      runState = WHACK_OVER_FLASH;
      gameOverFlashStartMs = now;
    } else {
      molePos = randomMolePos(molePos);
      moleSpawnMs = now;
    }
  }
}

const WhackSnapshot& getMoleSnapshot() {
  static WhackSnapshot snap;
  snap.state = runState;
  snap.countdownValue = countdownValue;
  snap.cursorPos = cursorPos;
  snap.molePos = molePos;
  snap.score = score;
  snap.bestScore = bestScore;
  snap.misses = misses;
  return snap;
}
