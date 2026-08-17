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

// Quick blip on a successful jump - deliberately tiny/non-blocking since
// it fires from inside the game's input handling, not a rare one-off.
void playGameJump();

// Single low blip on a non-fatal hit (lives remaining > 0 after it) -
// distinct from playGameJump() (higher, jump = good) so a miss reads as
// negative feedback without needing a screen change to notice it.
void playGameMiss();
