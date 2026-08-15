#pragma once

void initBuzzer();

// Short blip on every card change.
void beepCardChange();

// Longer chime, replayed on long-press. Shared across all cards for now
// (per plan: "can share one across all cards").
void playChime();
