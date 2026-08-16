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
  ANIM_SNOWFALL,   // no press animation - falling icons keep going continuously, screensaver-style
};

// Zero-valued members (CENTER/MIDDLE/no corner emoji) intentionally match
// today's fixed-layout look, so every existing card initializer in
// Cards.cpp keeps compiling unchanged - C++ zero-fills any trailing
// aggregate-initializer members that aren't explicitly listed.
enum TextAlignH : uint8_t { ALIGN_H_CENTER = 0, ALIGN_H_LEFT, ALIGN_H_RIGHT };
enum TextAlignV : uint8_t { ALIGN_V_MIDDLE = 0, ALIGN_V_TOP, ALIGN_V_BOTTOM };

// Each emoji shortcode belongs to one of these "families" for animation
// purposes - smile/smileb share one (they're the two faces of the same
// wink/grin cycle) and so do note/notes (single note <-> double note is
// the "spin"), so there are 5 toggles rather than 7. See
// glyphToEmojiAnimFamily() and drawBounce()/drawCornerEmoji() in
// DisplayUI.cpp for what each one actually looks like.
enum EmojiAnimFamily : uint8_t {
  EMOJI_ANIM_HEART = 0,
  EMOJI_ANIM_SMILE = 1,
  EMOJI_ANIM_SPARKLE = 2,
  EMOJI_ANIM_DIAMOND = 3,
  EMOJI_ANIM_NOTES = 4,
  EMOJI_ANIM_SNOWFLAKE = 5,
  EMOJI_ANIM_NONE = 255,
};

// Not an emoji family, but reuses the same Card::animatedEmojiDisableMask
// bitmask (see below) - unlike the families above, this one always shows
// a toggle in the site UI regardless of what's on the card, since the
// border-flash is drawn on every card using the bounce layout, not just
// ones with a specific emoji present.
#define BORDER_FLASH_DISABLE_BIT 6

struct Card {
  char text[CARD_TEXT_LEN];  // mutable - remote updates write directly here
  const uint8_t* bitmap;     // nullptr if this card has no icon bitmap
  uint8_t bmpW;
  uint8_t bmpH;
  CardAnimation animation;   // which short-press animation this card uses
  bool populated;            // false for reserved slots until filled remotely
  uint16_t animationDurationMs;  // how long this card's short-press animation runs

  // Only respected by the generic ANIM_BOUNCE layout (drawBounce() in
  // DisplayUI.cpp) - every other animation has its own bespoke hand-tuned
  // layout that doesn't use these. That's the reserved/custom-card slot,
  // i.e. the only card meant to be layout-configurable from the site.
  TextAlignH alignH;
  TextAlignV alignV;
  char cornerEmoji[4];  // CP437 byte per corner: TL, TR, BL, BR. 0 = none.

  // Bit i set (1 << EmojiAnimFamily i) = that family's default animation
  // is turned OFF for this card. Deliberately a "disable" mask, not an
  // "enable" one - zero-init (every existing card initializer, and a
  // freshly-populated reserved slot) then means "everything animates by
  // default," matching the site's per-card opt-*out* toggles rather than
  // needing every card to explicitly opt in.
  uint8_t animatedEmojiDisableMask;
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

// Sets text alignment for the card at the given index - only visible on
// cards using ANIM_BOUNCE (see Card's comment above).
void setCardAlignment(int index, TextAlignH alignH, TextAlignV alignV);

// Sets (or clears, with glyph 0) the CP437 corner-decoration emoji at one
// of the 4 corners (0=TL, 1=TR, 2=BL, 3=BR) - out-of-range corner indices
// are ignored.
void setCardCornerEmoji(int index, int corner, char glyph);

// Looks up one of the shortcodes in docs/remote-api-spec.md's emoji table
// (e.g. ":heart:") and returns its CP437 glyph byte, or 0 if the code is
// empty/unrecognized. Used for corner-emoji decorations (setCardCornerEmoji
// takes a raw glyph, not a shortcode) - RemoteControl.cpp converts.
char emojiShortcodeToGlyph(const char* code);

// Reverse of the above - glyph byte back to its shortcode string (or ""
// for 0/unrecognized). Used when reporting current card state back to the
// site (see sendCardsReport() in RemoteControl.cpp).
const char* glyphToEmojiShortcode(char glyph);

// Which animation family (if any) a glyph belongs to - EMOJI_ANIM_NONE
// for glyph 0 or anything not in the animatable set.
EmojiAnimFamily glyphToEmojiAnimFamily(char glyph);

// Sets which emoji animation families are disabled for this card - see
// Card::animatedEmojiDisableMask.
void setCardAnimatedEmojiDisableMask(int index, uint8_t disableMask);

// Walks from `from` in `direction` (+1 or -1), wrapping around, and
// returns the index of the next visible card - used so browsing with the
// knob skips over still-empty reserved slots, and the clock card when no
// time sync has ever succeeded this boot.
int nextVisibleIndex(int from, int direction);
