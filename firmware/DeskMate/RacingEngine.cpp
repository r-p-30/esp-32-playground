#include <Arduino.h>
#include "RacingEngine.h"
#include "Config.h"
#include "HighScores.h"

// NVS key for this game's persisted best score - <=15 chars, unique per game.
#define HIGHSCORE_KEY "racing"

static RacingRunState runState = RACING_COUNTDOWN;
static unsigned long countdownStartMs = 0;
static int countdownValue = 2;

static int playerLane = 0;

static RacingCar cars[RACING_MAX_OBSTACLES];
static int ticksSinceLastSpawn = 0;

static unsigned long lastTickMs = 0;
static unsigned long gameOverFlashStartMs = 0;

static int score = 0;
// Deliberately NOT reset by resetCommon() - persists across runs/restarts
// in RAM, and in NVS flash (HighScores.h), same as every other game's
// bestScore.
static int bestScore = 0;
static bool bestScoreLoaded = false;
static int bestScoreAtRunStart = 0;

// Two-part ramp (this task's explicit ask, not a uniform "every N points
// from zero" shape): flat at the base pace until score reaches
// RACING_SPEEDUP_FIRST_SCORE, then one step faster, held flat for
// RACING_SPEEDUP_STEP_SCORE points, then another step, and so on.
// Recomputed fresh off current score rather than stored, same approach as
// Snake's/Whack's difficulty ramps.
static unsigned long currentTickIntervalMs() {
  int steps = 0;
  if (score >= RACING_SPEEDUP_FIRST_SCORE) {
    steps = 1 + (score - RACING_SPEEDUP_FIRST_SCORE) / RACING_SPEEDUP_STEP_SCORE;
  }
  long ms = (long)RACING_TICK_INTERVAL_MS - steps * (long)RACING_SPEEDUP_STEP_MS;
  if (ms < (long)RACING_TICK_MIN_MS) ms = (long)RACING_TICK_MIN_MS;
  return (unsigned long)ms;
}

static void clearCars() {
  for (int i = 0; i < RACING_MAX_OBSTACLES; i++) cars[i].active = false;
}

static void resetCommon() {
  if (!bestScoreLoaded) {
    bestScoreLoaded = true;
    bestScore = loadHighScore(HIGHSCORE_KEY);
  }
  bestScoreAtRunStart = bestScore;

  runState = RACING_COUNTDOWN;
  countdownStartMs = millis();
  countdownValue = 2;

  playerLane = 0;
  clearCars();
  ticksSinceLastSpawn = RACING_MIN_SPAWN_GAP_TICKS;  // eligible to spawn as soon as RUNNING starts, not after an extra wait

  lastTickMs = 0;  // next stepRacingGame() call ticks fresh, not a huge catch-up jump

  score = 0;
  // bestScore intentionally untouched.
}

void initRacingGame() { resetCommon(); }
void racingRestart() { resetCommon(); }

void racingMoveLane(int direction) {
  if (runState != RACING_RUNNING) return;
  int next = playerLane + (direction > 0 ? 1 : -1);
  if (next < 0) next = 0;
  if (next > RACING_LANES - 1) next = RACING_LANES - 1;
  playerLane = next;
}

void racingTogglePause() {
  if (runState == RACING_RUNNING) {
    runState = RACING_PAUSED;
  } else if (runState == RACING_PAUSED) {
    runState = RACING_RUNNING;
    lastTickMs = millis();  // resume waits one fresh interval, doesn't jump instantly on the same frame it un-pauses
  }
}

bool isRacingGameOver() { return runState == RACING_OVER; }

// Picks a free obstacle slot (inactive), or -1 if the pool's full (can
// only happen with RACING_MAX_OBSTACLES active cars at once, i.e. every
// lane already occupied).
static int findFreeSlot() {
  for (int i = 0; i < RACING_MAX_OBSTACLES; i++) {
    if (!cars[i].active) return i;
  }
  return -1;
}

void stepRacingGame() {
  unsigned long now = millis();

  if (runState == RACING_OVER || runState == RACING_PAUSED) return;  // frozen until racingRestart()/racingTogglePause()

  if (runState == RACING_OVER_FLASH) {
    if (now - gameOverFlashStartMs >= GAME_OVER_FLASH_DURATION_MS) {
      runState = RACING_OVER;
    }
    return;
  }

  if (runState == RACING_COUNTDOWN) {
    unsigned long elapsed = now - countdownStartMs;
    int phase = (int)(elapsed / GAME_COUNTDOWN_PHASE_MS);
    if (phase > 2) phase = 2;
    countdownValue = 2 - phase;

    if (elapsed >= GAME_COUNTDOWN_PHASE_MS * 3) {
      runState = RACING_RUNNING;
      lastTickMs = now;
    }
    return;
  }

  // RACING_RUNNING from here down - fixed-cadence row-step tick, not a
  // per-frame move.
  if (now - lastTickMs < currentTickIntervalMs()) return;
  lastTickMs = now;

  ticksSinceLastSpawn++;

  // Advance every active obstacle by exactly one row. A collision return
  // leaves the rest of the pool one tick stale - harmless, the world is
  // about to freeze in RACING_OVER_FLASH anyway (same "stop right at the
  // hit" reasoning as SnakeEngine.cpp's stepSnakeGame()).
  for (int i = 0; i < RACING_MAX_OBSTACLES; i++) {
    if (!cars[i].active) continue;
    cars[i].row++;
    if (cars[i].row == RACING_PLAYER_ROW) {
      if (cars[i].lane == playerLane) {
        runState = RACING_OVER_FLASH;
        gameOverFlashStartMs = now;
        if (bestScore > bestScoreAtRunStart) {
          saveHighScore(HIGHSCORE_KEY, bestScore);
        }
        return;  // world stays exactly as it was at the moment of the hit
      }
      // Different lane - dodged. This obstacle is done.
      cars[i].active = false;
      score++;
      if (score > bestScore) bestScore = score;
    }
  }

  // Spawn - see RACING_MIN_SPAWN_GAP_TICKS's comment (Config.h) for why a
  // shared gap counter (not per-lane) is what keeps this always fair.
  //
  // Lane pick is a plain independent random(0, RACING_LANES) -
  // deliberately NOT excluded from lanes that already have an active car.
  // An earlier pass here tried restricting the pick to only-currently-free
  // lanes, but with few lanes to choose from that collapses to exactly one
  // legal choice as soon as most lanes have a car in them - i.e. it forces
  // a mechanical, fully deterministic alternation instead of picking
  // randomly at all. A genuinely independent pick can (and should)
  // occasionally double up a lane or leave one empty for a few spawns in a
  // row - that clustering is what real randomness looks like, not a bug to
  // engineer away.
  if (ticksSinceLastSpawn >= RACING_MIN_SPAWN_GAP_TICKS) {
    int slot = findFreeSlot();
    if (slot >= 0) {
      cars[slot].active = true;
      cars[slot].row = 0;
      cars[slot].lane = (int)random(0, RACING_LANES);
      ticksSinceLastSpawn = 0;
    }
  }
}

const RacingSnapshot& getRacingSnapshot() {
  static RacingSnapshot snap;
  snap.state = runState;
  snap.countdownValue = countdownValue;
  snap.playerLane = playerLane;
  for (int i = 0; i < RACING_MAX_OBSTACLES; i++) snap.cars[i] = cars[i];
  snap.score = score;
  snap.bestScore = bestScore;
  return snap;
}
