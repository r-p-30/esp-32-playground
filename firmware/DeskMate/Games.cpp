#include "Game.h"
#include "GameEngine.h"
#include "DisplayUI.h"
#include "BuzzerFX.h"

// ---- Dino Jump ----
// Thin wrapper around the existing GameEngine.cpp/DisplayUI.cpp
// implementation - none of that logic changed, this just exposes it
// through the shared Game interface. Bodies below are copied verbatim from
// what DeskMate.ino used to do inline for MODE_GAME.

static void dinoEnter() {
  initGame();
  showGameFrame();
}

static void dinoStep() {
  updateGameFrame();  // already throttles itself to GAME_FRAME_INTERVAL_MS
}

static void dinoOnShortPress() {
  if (isGameOver()) {
    gameRestart();
    showGameFrame();
  } else if (getGameSnapshot().state == GAME_RUNNING) {
    gameJump();
    playGameJump();
  }
}

static bool dinoIsOver() {
  return isGameOver();
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
static bool stubIsOver() { return false; }
static int stubBestScore() { return 0; }

const Game GAMES[GAME_COUNT] = {
  { "Dino Jump",    true,  dinoEnter, dinoStep, dinoOnShortPress, dinoIsOver, dinoBestScore },
  { "Whack-a-mole", false, stubEnter, stubStep, stubOnShortPress, stubIsOver, stubBestScore },
  { "Snake",        false, stubEnter, stubStep, stubOnShortPress, stubIsOver, stubBestScore },
  { "Tetris",       false, stubEnter, stubStep, stubOnShortPress, stubIsOver, stubBestScore },
};
