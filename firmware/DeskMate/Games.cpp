#include "Game.h"
#include "GameEngine.h"
#include "WhackEngine.h"
#include "DisplayUI.h"
#include "BuzzerFX.h"
#include "Config.h"

// ---- Dino Jump ----
// Thin wrapper around the existing GameEngine.cpp/DisplayUI.cpp
// implementation - none of that logic changed, this just exposes it
// through the shared Game interface. Bodies below are copied verbatim from
// what DeskMate.ino used to do inline for MODE_GAME.

// Edge-detection state for dinoStep()'s sound triggers below - not part of
// GameEngine.cpp's own state since that file deliberately has no sound
// calls in it (see its collision-handling comment). Stale values left over
// from a previous run are harmless: both checks only fire on a specific
// transition (state just became GAME_OVER_FLASH; lives just dropped but
// isn't zero), so at worst one "impossible" comparison is skipped on the
// first step() of a fresh run - reset anyway in dinoEnter() for clarity.
static GameRunState dinoPrevState = GAME_COUNTDOWN;
static int dinoPrevLives = GAME_LIVES;

static void dinoEnter() {
  initGame();
  dinoPrevState = GAME_COUNTDOWN;
  dinoPrevLives = GAME_LIVES;
  showGameFrame();
}

static void dinoStep() {
  updateGameFrame();  // already throttles itself to GAME_FRAME_INTERVAL_MS

  const GameSnapshot& snap = getGameSnapshot();
  if (snap.state == GAME_OVER_FLASH && dinoPrevState != GAME_OVER_FLASH) {
    // Last life just lost - alarm rings for the whole GAME_OVER_FLASH
    // blink window, not just a couple beeps at the start of it.
    playGameOverAlarm();
  } else if (snap.lives < dinoPrevLives && snap.lives > 0) {
    // Non-fatal hit - lives dropped but the run isn't over.
    playGameMiss();
  }
  dinoPrevState = snap.state;
  dinoPrevLives = snap.lives;
}

static void dinoOnShortPress() {
  if (isGameOver()) {
    gameRestart();
    dinoPrevState = GAME_COUNTDOWN;
    dinoPrevLives = GAME_LIVES;
    showGameFrame();
  } else if (getGameSnapshot().state == GAME_RUNNING) {
    gameJump();
    playGameJump();
  }
}

// Dino has no use for rotation - the only input it reads is press (jump).
static void dinoOnRotate(int direction) {
  (void)direction;
}

static int dinoBestScore() {
  return getGameSnapshot().bestScore;
}

// ---- Whack-a-mole ----
// Same wrapper shape as Dino above: WhackEngine.cpp/DisplayUI.cpp own the
// actual physics/drawing, this just exposes it through the shared Game
// interface and owns the edge-detection needed to fire sounds at the right
// moments (WhackEngine.cpp itself has no sound calls, same reasoning as
// GameEngine.cpp's).
static WhackRunState whackPrevState = WHACK_COUNTDOWN;
static int whackPrevMisses = 0;

static void whackEnter() {
  initMoleGame();
  whackPrevState = WHACK_COUNTDOWN;
  whackPrevMisses = 0;
  showMoleFrame();
}

static void whackStep() {
  updateMoleFrame();  // already throttles itself to GAME_FRAME_INTERVAL_MS

  const WhackSnapshot& snap = getMoleSnapshot();
  if (snap.state == WHACK_OVER_FLASH && whackPrevState != WHACK_OVER_FLASH) {
    // Miss limit just hit - alarm rings for the whole GAME_OVER_FLASH
    // blink window, same as Dino's.
    playGameOverAlarm();
  } else if (snap.misses > whackPrevMisses) {
    // A mole just timed out (not the game-ending one, that's covered by
    // the branch above instead).
    playGameMiss();
  }
  whackPrevState = snap.state;
  whackPrevMisses = snap.misses;
}

static void whackOnShortPress() {
  if (isMoleGameOver()) {
    moleRestart();
    whackPrevState = WHACK_COUNTDOWN;
    whackPrevMisses = 0;
    showMoleFrame();
    return;
  }

  int scoreBefore = getMoleSnapshot().score;
  moleWhack();
  if (getMoleSnapshot().score > scoreBefore) {
    // Hit - reuse the existing high-pitched "jump" tone as the hit beep,
    // it already matches the plan doc's "short high-pitched beep on hit."
    playGameJump();
    showMoleFrame();  // redraw immediately so the respawned mole shows without waiting for the next throttled tick
  }
}

static void whackOnRotate(int direction) {
  moleMoveCursor(direction);
  // Redraw immediately so the cursor move is visible right away, same
  // immediacy reasoning as the game menu's ENC_NEXT/ENC_PREV handling in
  // DeskMate.ino redrawing synchronously rather than waiting for the next
  // tick. moleMoveCursor() itself is a no-op outside WHACK_RUNNING, so this
  // redraw is harmless (identical frame) during countdown/game-over.
  showMoleFrame();
}

static int whackBestScore() {
  return getMoleSnapshot().bestScore;
}

// ---- Not implemented yet ----
// Selectable in the menu, but `implemented: false` means DeskMate.ino never
// actually calls these - kept as real no-ops rather than nullptr just so
// GAMES[] never holds a dangling function pointer.
static void stubEnter() {}
static void stubStep() {}
static void stubOnShortPress() {}
static void stubOnRotate(int direction) { (void)direction; }
static int stubBestScore() { return 0; }

const Game GAMES[GAME_COUNT] = {
  { "Dino Jump",    true,  dinoEnter,  dinoStep,  dinoOnShortPress,  dinoOnRotate,  dinoBestScore },
  { "Whack-a-mole", true,  whackEnter, whackStep, whackOnShortPress, whackOnRotate, whackBestScore },
  { "Snake",        false, stubEnter,  stubStep,  stubOnShortPress,  stubOnRotate,  stubBestScore },
  { "Tetris",       false, stubEnter,  stubStep,  stubOnShortPress,  stubOnRotate,  stubBestScore },
};
