#pragma once

// Physics/state for the whack-a-mole game mode. Deliberately has no
// display code in here - same split as GameEngine.h (Dino's physics/state)
// vs DisplayUI.cpp (drawing). Own enum/struct names (Whack*, not Game*)
// since both this file and GameEngine.h are compiled into the same sketch.

enum WhackRunState {
  WHACK_COUNTDOWN,    // "Ready" / "Set" / "Go" intro, no mole yet
  WHACK_RUNNING,
  // Last allowed miss just happened - world frozen (mole stays exactly
  // where it was, whether just-hit or just-timed-out), DisplayUI.cpp
  // blinks a "GAME OVER" box over it for GAME_OVER_FLASH_DURATION_MS
  // (Config.h), then stepMoleGame() itself advances to WHACK_OVER once
  // that elapses. Same shape as GameEngine.h's GAME_OVER_FLASH.
  WHACK_OVER_FLASH,
  WHACK_OVER          // resting score card - press to retry
};

// Read-only snapshot for DisplayUI's renderer - plain fields rather than
// per-value getters since every field is needed together, every frame.
struct WhackSnapshot {
  WhackRunState state;
  int countdownValue;  // 2/1/0, only meaningful during WHACK_COUNTDOWN
  int cursorPos;        // 0..MOLE_POSITIONS-1 (Config.h), player's aimed hole
  int molePos;           // 0..MOLE_POSITIONS-1, -1 during countdown (no mole yet)
  int score;              // hits this run
  int bestScore;           // highest score seen since boot, same persistence as Dino's
  int misses;               // timed-out moles this run, 0..MOLE_MAX_MISSES (Config.h)
};

// Resets to a fresh run: WHACK_COUNTDOWN state, cursor back to hole 0,
// score/misses back to 0. Does NOT touch bestScore - persists in RAM
// across runs (and across mode switches) until reboot/reflash, same as
// Dino's initGame(). Called on entering game mode.
void initMoleGame();

// Alias for initMoleGame() - used when a short press on the game-over
// screen starts a new run. Also replays the Ready/Set/Go countdown.
void moleRestart();

// Moves the cursor one hole in the given direction (+1/-1), wrapping
// around MOLE_POSITIONS. No-op unless the game is actively running - same
// guard reasoning as GameEngine.h's gameJump(), so a stray rotation can't
// do anything during the countdown or after game over.
void moleMoveCursor(int direction);

// Attempts a hit at the cursor's current hole. No-op unless actively
// running. If the cursor is on the mole: score++, mole immediately
// respawns elsewhere. If the cursor isn't on the mole: no-op - a whiffed
// press does NOT count as a miss, only a timed-out mole does (see
// stepMoleGame()) - matches docs/extended-game-plan.md's own definition of
// a miss.
void moleWhack();

// Advances physics by however much real time has elapsed since the last
// call (millis()-based, not tick-based), same reasoning as GameEngine.h's
// stepGame(). Caller (DisplayUI's updateMoleFrame()) is responsible for
// throttling call frequency (GAME_FRAME_INTERVAL_MS, Config.h).
void stepMoleGame();

// True only once the WHACK_OVER_FLASH blink has finished and the resting
// score card is showing - i.e. exactly when a short press should restart.
bool isMoleGameOver();

const WhackSnapshot& getMoleSnapshot();
