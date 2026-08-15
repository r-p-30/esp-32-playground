#pragma once
#include <Arduino.h>

struct Card {
  const char* text;        // shown on screen, keep to ~4-6 lines at 128x64
  const uint8_t* bitmap;    // nullptr if this card has no icon
  uint8_t bmpW;
  uint8_t bmpH;
};

extern const Card cards[];
extern const uint8_t NUM_CARDS;

// Returns the card at the given index (wraps if out of range).
// Kept as its own function per the plan's phase-2 note: this is the seam
// where phase 2 can swap in a fetch from the hosted config site later.
const Card& getCardContent(int index);
