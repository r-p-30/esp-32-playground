#pragma once

void initBuzzer();

// Short blip on every card change.
void beepCardChange();

// Longer chime, played whenever the device switches mode (cards/game,
// or in/out of night mode).
void playChime();

// Distinct alert pattern for an incoming remote message/buzz - deliberately
// different from beepCardChange() and playChime() so it stands out.
void playIncomingMessage();

// Two quick identical beeps for the remote "identify" ping - confirms an
// update reached the device without touching card content.
void playIdentifyPing();

// Single low tone sustained for the full GAME_OVER_FLASH_DURATION_MS
// (Config.h) - fired once, right as any game enters its GAME_OVER_FLASH
// state, so the buzzer rings for the whole "GAME OVER" blink instead of
// just a couple short beeps at the start of it. Shared across every game
// (Dino, Whack-a-mole, ...) rather than each having its own game-over
// sound. Non-blocking - tone() with a duration returns immediately and
// keeps sounding in the background, same as playGameJump()/playGameMiss().
void playGameOverAlarm();

// Quick blip on a successful jump - deliberately tiny/non-blocking since
// it fires from inside the game's input handling, not a rare one-off.
void playGameJump();

// Single low blip on a non-fatal hit (lives remaining > 0 after it) -
// distinct from playGameJump() (higher, jump = good) so a miss reads as
// negative feedback without needing a screen change to notice it.
void playGameMiss();

// Tetris line clear - pitch and duration both scale up with `lines`
// (1-4 simultaneous), so a multi-line clear reads as more of an event than
// a single one, per docs/game-mode-plan.md's Tetris feedback note.
// Non-blocking, same reasoning as playGameJump()/playGameMiss().
void playGameLineClear(int lines);

// Car Racing's 2->3 lane widen - single bright blip, pitched above
// playGameJump() so it reads as a milestone rather than another gameplay
// blip. Non-blocking, same reasoning as the other in-game sounds above.
void playGameLevelUp();
