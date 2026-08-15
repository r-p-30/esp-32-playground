#pragma once
#include <Arduino.h>

#define CARD_TEXT_LEN 160

enum CardAnimation : uint8_t {
  ANIM_BOUNCE,        // generic fallback - used for the reserved empty slots
  ANIM_PACMAN,
  ANIM_FLOWER,
  ANIM_TERMINAL,
  ANIM_TERMINAL_PASSWORD,  // like ANIM_TERMINAL, but types out trailing '*' characters
  ANIM_TERMINAL_TYPE_STATUS,    // types out "200 OK"
  ANIM_TERMINAL_TYPE_BUGS,      // types out "0.0"
  ANIM_TERMINAL_TYPE_FIREWALL,  // types out "my_heart"
  ANIM_HEART,
  ANIM_SUNRISE,
  ANIM_MOON_SPARKLE,
  ANIM_TIME_SWIRL,
  ANIM_BOX_BURST,
  ANIM_COUNTER,
  ANIM_CONSTELLATION,
  ANIM_MAP_PIN,
  ANIM_DINO_RUN,
  ANIM_PULSE_BOX,
  ANIM_PEEKABOO,
  ANIM_FLIRT,
  ANIM_CLOCK,
  ANIM_DUCK_FLUSH,
  ANIM_EQUALIZER,  // no press animation - bars move continuously on their own
};

struct Card {
  char text[CARD_TEXT_LEN];  // mutable - remote updates write directly here
  const uint8_t* bitmap;     // nullptr if this card has no icon bitmap
  uint8_t bmpW;
  uint8_t bmpH;
  CardAnimation animation;   // which short-press animation this card uses
  bool populated;            // false for reserved slots until filled remotely
  uint16_t animationDurationMs;  // how long this card's short-press animation runs
};

extern Card cards[];
extern const uint8_t NUM_CARDS;

// Returns the card at the given index (wraps if out of range).
const Card& getCardContent(int index);

// Overwrites the text of the card at the given index (wraps if out of
// range) - used by the remote-control feature (RemoteControl.cpp) to
// edit an existing card, or to populate one of the reserved empty slots
// as an "added" card. Marks the slot as populated. Local/offline content
// is untouched until this is called, and a reboot always comes back to
// the defaults in Cards.cpp.
void setCardText(int index, const char* text);

// Overwrites how long (ms) this card's short-press animation runs - lets
// the hosted site remotely tune timings for testing without reflashing.
// 0 or absurdly large values are left as-is by the caller; this just
// stores whatever it's given.
void setCardAnimationDuration(int index, uint16_t durationMs);

// Walks from `from` in `direction` (+1 or -1), wrapping around, and
// returns the index of the next visible card - used so browsing with the
// knob skips over still-empty reserved slots, and the clock card when no
// time sync has ever succeeded this boot.
int nextVisibleIndex(int from, int direction);
