#include <Arduino.h>
#include <math.h>
#include "GameEngine.h"
#include "Config.h"

// Dino hitbox - a simplified bounding box around the small sprite
// DisplayUI.cpp draws (excludes the decorative tail spike, same reasoning
// as the cactus hitboxes below). DINO_BASE_X must match the dinoX
// DisplayUI.cpp renders at, so the two stay visually in sync.
#define DINO_BASE_X               26
#define DINO_HITBOX_CENTER_OFFSET 2
#define DINO_HITBOX_HALF_W        6

// Cactus hitboxes - slightly generous vs. the drawn sprite (which also has
// thin arm spikes), so collisions read as fair rather than "that barely
// grazed it."
#define CACTUS_SMALL_HALF_W  4
#define CACTUS_SMALL_H        14
#define CACTUS_BIG_HALF_W    5
#define CACTUS_BIG_H          20

// World-space gap (px) after the current cactus fully exits, before the
// next one spawns - single obstacle in play at a time (see GameCactus in
// GameEngine.h), so this is what makes the next one "anticipatable"
// instead of appearing right on top of the last one landing. Generous on
// purpose - a rotary-encoder button needs more setup time per jump than a
// mouse click would, so a tight rhythm here reads as unfair rather than
// challenging.
#define SPAWN_GAP_MIN_PX  110
#define SPAWN_GAP_MAX_PX  190

static GameRunState runState = GAME_COUNTDOWN;
static unsigned long countdownStartMs = 0;
static int countdownValue = 2;

static bool airborne = false;
static unsigned long jumpStartMs = 0;
// Set once per jump (in gameJump()), not a fixed constant - see gameJump().
static int jumpPeakHeightPx = GAME_JUMP_PEAK_SMALL_PX;
static int dinoHeightPx = 0;
static int dinoForwardOffsetPx = 0;

static bool legFrameA = true;
static unsigned long legFrameTimerMs = 0;

static GameCactus cactus;
static float distanceUntilNextSpawn = 0;

static float scoreAccumPx = 0;
static int score = 0;
// Deliberately NOT reset by resetCommon() - persists across runs/restarts
// until reboot or reflash, per docs/desk-mate-project-plan.md.
static int bestScore = 0;

static unsigned long lastStepMs = 0;

static float randomSpawnGap() {
  return (float)random(SPAWN_GAP_MIN_PX, SPAWN_GAP_MAX_PX + 1);
}

static void resetCommon() {
  runState = GAME_COUNTDOWN;
  countdownStartMs = millis();
  countdownValue = 2;

  airborne = false;
  jumpStartMs = 0;
  jumpPeakHeightPx = GAME_JUMP_PEAK_SMALL_PX;
  dinoHeightPx = 0;
  dinoForwardOffsetPx = 0;

  legFrameA = true;
  legFrameTimerMs = 0;

  cactus.active = false;
  cactus.x = 0;
  cactus.big = false;
  distanceUntilNextSpawn = 0;  // set for real once GAME_RUNNING starts

  scoreAccumPx = 0;
  score = 0;
  // bestScore intentionally untouched.

  lastStepMs = 0;  // next stepGame() call computes dt fresh, not a huge jump
}

void initGame() {
  resetCommon();
}

void gameRestart() {
  resetCommon();
}

void gameJump() {
  if (runState != GAME_RUNNING) return;  // no jumping during countdown or after game over
  if (airborne) return;
  airborne = true;
  jumpStartMs = millis();
  // The big cactus (20px) needs real height to clear; the small one (14px)
  // only needs a little. A single fixed peak either clips the big one or
  // floats needlessly (and less satisfyingly) over the small one - so the
  // arc adapts to whichever obstacle is actually in play right now.
  jumpPeakHeightPx = (cactus.active && cactus.big) ? GAME_JUMP_PEAK_BIG_PX : GAME_JUMP_PEAK_SMALL_PX;
}

bool isGameOver() {
  return runState == GAME_OVER;
}

void stepGame() {
  unsigned long now = millis();
  if (lastStepMs == 0) lastStepMs = now;
  unsigned long dtMs = now - lastStepMs;
  lastStepMs = now;

  if (runState == GAME_OVER) return;  // frozen until gameRestart()

  if (runState == GAME_COUNTDOWN) {
    unsigned long elapsed = now - countdownStartMs;
    int phase = (int)(elapsed / GAME_COUNTDOWN_PHASE_MS);
    if (phase > 2) phase = 2;
    countdownValue = 2 - phase;

    if (elapsed >= GAME_COUNTDOWN_PHASE_MS * 3) {
      runState = GAME_RUNNING;
      distanceUntilNextSpawn = randomSpawnGap();
    }
    return;
  }

  // GAME_RUNNING from here down.
  float dtSec = (float)dtMs / 1000.0f;

  if (airborne) {
    unsigned long jumpElapsed = now - jumpStartMs;
    float t = (float)jumpElapsed / (float)GAME_JUMP_DURATION_MS;
    if (t >= 1.0f) {
      airborne = false;
      dinoHeightPx = 0;
      dinoForwardOffsetPx = 0;
    } else {
      // Parabola (0 at takeoff/landing, peak at t=0.5) for height, and a
      // half-sine for the forward nudge - gives the jump a real "arc
      // forward and up" feel instead of a straight vertical bounce.
      dinoHeightPx = (int)(jumpPeakHeightPx * 4.0f * t * (1.0f - t));
      dinoForwardOffsetPx = (int)(GAME_JUMP_FORWARD_PX * sinf(PI * t));
    }
  }

  legFrameTimerMs += dtMs;
  if (legFrameTimerMs >= 150) {
    legFrameTimerMs = 0;
    legFrameA = !legFrameA;
  }

  float scrollDelta = GAME_SCROLL_SPEED_PX_S * dtSec;

  if (cactus.active) {
    cactus.x -= scrollDelta;
    if (cactus.x < -20) {
      cactus.active = false;
      distanceUntilNextSpawn = randomSpawnGap();
    }
  } else {
    distanceUntilNextSpawn -= scrollDelta;
    if (distanceUntilNextSpawn <= 0) {
      cactus.active = true;
      cactus.x = (float)OLED_WIDTH + 4;
      cactus.big = random(0, 2) == 1;
    }
  }

  scoreAccumPx += scrollDelta;
  score = (int)(scoreAccumPx / 4.0f);
  if (score > bestScore) bestScore = score;

  if (cactus.active) {
    float dinoCenterX = DINO_BASE_X + DINO_HITBOX_CENTER_OFFSET + dinoForwardOffsetPx;
    float cactusHalfW = cactus.big ? CACTUS_BIG_HALF_W : CACTUS_SMALL_HALF_W;
    float cactusH = cactus.big ? CACTUS_BIG_H : CACTUS_SMALL_H;

    bool horizOverlap = fabsf(dinoCenterX - cactus.x) < (DINO_HITBOX_HALF_W + cactusHalfW);
    // Forgiveness knocked off the required height, not added to the
    // hitbox width - a near-miss on timing (jumped a beat early/late)
    // should still count as cleared; a genuine same-height collision
    // shouldn't become unwinnable-feeling on top of it.
    bool notCleared = dinoHeightPx < ((int)cactusH - GAME_JUMP_CLEARANCE_FORGIVENESS_PX);
    if (horizOverlap && notCleared) {
      runState = GAME_OVER;
    }
  }
}

const GameSnapshot& getGameSnapshot() {
  static GameSnapshot snap;
  snap.state = runState;
  snap.countdownValue = countdownValue;
  snap.dinoHeightPx = dinoHeightPx;
  snap.dinoForwardOffsetPx = dinoForwardOffsetPx;
  snap.legFrameA = legFrameA;
  snap.cactus = cactus;
  snap.score = score;
  snap.bestScore = bestScore;
  return snap;
}
