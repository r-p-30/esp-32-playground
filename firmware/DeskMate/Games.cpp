#include "Game.h"
#include "GameEngine.h"
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
    // Last life just lost - same beep pattern as the site's "identify"
    // ping, reused verbatim per how this was asked for.
    playIdentifyPing();
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

static int dinoBestScore() {
  return getGameSnapshot().bestScore;
}

// ---- Not implemented yet ----
// Selectable in the menu, but `implemented: false` means DeskMate.ino never
// actually calls these - kept as real no-ops rather than nullptr just so
// GAMES[] never holds a dangling function pointer.
static void stubEnter() {}
static void stubStep() {}
static void stubOnShortPress() {}
static int stubBestScore() { return 0; }

const Game GAMES[GAME_COUNT] = {
  { "Dino Jump",    true,  dinoEnter, dinoStep, dinoOnShortPress, dinoBestScore },
  { "Whack-a-mole", false, stubEnter, stubStep, stubOnShortPress, stubBestScore },
  { "Snake",        false, stubEnter, stubStep, stubOnShortPress, stubBestScore },
  { "Tetris",       false, stubEnter, stubStep, stubOnShortPress, stubBestScore },
};
