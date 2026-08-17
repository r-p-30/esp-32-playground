#pragma once

// Shared interface every mini-game implements (see GAMES[] in Games.cpp), so
// the game-menu state machine in DeskMate.ino can enter/step/press-handle
// whichever one is currently selected without knowing its internals. Mirrors
// the existing Cards.cpp (data) vs DisplayUI.cpp (drawing) split in this
// codebase - DisplayUI.cpp stays the only file that touches the `display`
// object, so each game's own draw calls stay routed through DisplayUI.h
// functions (see Games.cpp's Dino wrapper for the pattern).

enum GameId {
  GAME_DINO_JUMP = 0,
  GAME_WHACK_A_MOLE,
  GAME_SNAKE,
  GAME_TETRIS,
  GAME_COUNT
};

// Every game is expected to follow the same lifecycle beat, even though
// DeskMate.ino stays oblivious to the details behind enter()/step()/
// onShortPress(): a Ready/Set/Go countdown, then play, then on loss a
// brief frozen-frame "GAME OVER" flash (blink + a beep, under 2s) before
// settling on a resting score card that a short press restarts from. See
// Games.cpp's Dino wrapper (GameEngine.cpp's GAME_COUNTDOWN /
// GAME_RUNNING / GAME_OVER_FLASH / GAME_OVER states) for the reference
// implementation of that pattern - keep new games consistent with it.
struct Game {
  const char* name;
  // false = listed in the menu but not playable yet; selecting it is a
  // no-op (see enterSelectedGame() in DeskMate.ino) and the menu shows a
  // "(soon)" suffix if it fits on screen (DisplayUI.cpp's showGameMenu()).
  bool implemented;

  void (*enter)();         // reset to a fresh run and draw the first frame; called once on selecting from the menu
  void (*step)();          // advance by one tick and redraw; called every loop() while this game is the active one - also this game's own job to fire its own transition sounds (loss beep, miss blip, ...), see Games.cpp's dinoStep()
  void (*onShortPress)();  // per-game short-press behavior (jump/restart, whack a mole, rotate a piece, ...)
  int (*bestScore)();      // this game's own high-water mark - kept separate per game, not shared
};

extern const Game GAMES[GAME_COUNT];
