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

// Single soft ping for the random "notification" feature - distinct from
// the other three so it reads as ambient, not a real incoming message.
void playRandomPing();

// Two quick identical beeps for the remote "identify" ping - confirms an
// update reached the device without touching card content.
void playIdentifyPing();
