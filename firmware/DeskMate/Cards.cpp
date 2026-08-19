#include "Cards.h"
#include "TimeSync.h"
#include "Config.h"
#include <string.h>

// Default/fallback content - what the device shows entirely offline, and
// what every card starts as before any remote edits arrive. Remote
// updates only overwrite the .text buffer in RAM, never this table, so a
// reboot always comes back to exactly this. The reserved slots start
// with populated=false so browsing skips them until the hosted site
// fills one in (see setCardText() / nextPopulatedIndex()).
//
// animationDurationMs is per-card and remotely tunable (setCardAnimationDuration())
// so timings can be tested live without reflashing - ANIMATION_DURATION_MS
// (Config.h) is the standard length; a few cards need more room and say so below.
Card cards[] = {
  // Shown at boot only if syncTime() actually landed a real timestamp this
  // boot (see setup() in DeskMate.ino) - otherwise boot falls back to
  // "good morning" (index 2) below instead. No press animation - it's a
  // live clock, always ticking on its own. Also hidden from knob browsing
  // while unsynced (see isCardVisible() below).
  { "right now", nullptr, 0, 0, ANIM_CLOCK, true, ANIMATION_DURATION_MS },

  // Longer than the standard duration - there's two things happening at
  // once (balloons rising, confetti falling) and the default 1000ms
  // barely gives either room to read before the animation ends.
  { "HAPPY\nBIRTHDAY", nullptr, 0, 0, ANIM_BIRTHDAY, true, 1800 },

  // Boot fallback when card 0 (clock) isn't usable yet - also where
  // exitNightMode() always lands (see setNightMode() in DeskMate.ino;
  // index shifted from 1 to 2 when the birthday card above was inserted).
  { "good morning", nullptr, 0, 0, ANIM_SUNRISE, true, ANIMATION_DURATION_MS },

  { "wake up and\nuse more than\n2 neurons\ntoday :)", nullptr, 0, 0, ANIM_PULSE_BOX, true, ANIMATION_DURATION_MS },

  { "chomp chomp", nullptr, 0, 0, ANIM_PACMAN, true, 3000 },  // slower, longer maze traversal

  { "fool for you", nullptr, 0, 0, ANIM_FLOWER, true, ANIMATION_DURATION_MS },
   
  // No press animation either - the bars move continuously on their own,
  // same live-redraw mechanism as the clock card.
  { "music time !!!!", nullptr, 0, 0, ANIM_EQUALIZER, true, ANIMATION_DURATION_MS },

  { "sudo make_me\nhappy\n\nsudo send pw\nto me: ******", nullptr, 0, 0, ANIM_TERMINAL_PASSWORD, true, ANIMATION_DURATION_MS },

  { "i miss you", nullptr, 0, 0, ANIM_HEART, true, ANIMATION_DURATION_MS },

  { "You: zero bugs\nfound in this\nfriendship.\nCVE score: 0.0", nullptr, 0, 0, ANIM_TERMINAL_TYPE_BUGS, true, ANIMATION_DURATION_MS },

  { "action. magic.\ntime bending.", nullptr, 0, 0, ANIM_TIME_SWIRL, true, ANIMATION_DURATION_MS },

  { "firewall rule:\nALLOW you\nFROM anywhere\nTO my_heart", nullptr, 0, 0, ANIM_TERMINAL_TYPE_FIREWALL, true, ANIMATION_DURATION_MS },

  { "connect the\ndots", nullptr, 0, 0, ANIM_CONSTELLATION, true, ANIMATION_DURATION_MS },

  { "Status: 200 OK\nUptime: since\nday one.\nNo patches\nneeded.", nullptr, 0, 0, ANIM_TERMINAL_TYPE_STATUS, true, ANIMATION_DURATION_MS },

  { "here are some\nhappy thoughts", nullptr, 0, 0, ANIM_BOX_BURST, true, ANIMATION_DURATION_MS },

  { "friendship.log", nullptr, 0, 0, ANIM_COUNTER, true, ANIMATION_DURATION_MS },

  { "home is where\nyou are", nullptr, 0, 0, ANIM_MAP_PIN, true, ANIMATION_DURATION_MS },

  { "psst... there's\nmore", nullptr, 0, 0, ANIM_DINO_RUN, true, ANIMATION_DURATION_MS },

  { "hehe\n\n;)  xD  :P", nullptr, 0, 0, ANIM_PEEKABOO, true, ANIMATION_DURATION_MS },

  { "how you doin'", nullptr, 0, 0, ANIM_FLIRT, true, ANIMATION_DURATION_MS },

  { "shit happens!\nflush it &\nmove on", nullptr, 0, 0, ANIM_DUCK_FLUSH, true, ANIMATION_DURATION_MS },
    
  // Ambient screensaver - no press animation, just keeps falling on its
  // own like the equalizer/clock cards above (see updateDisplayAnimation()
  // in DisplayUI.cpp).
  { "let it snow", nullptr, 0, 0, ANIM_SNOWFALL, true, ANIMATION_DURATION_MS },

  // Reserved empty slot - fill this from the hosted site to "add" a new
  // card without reflashing (see docs/remote-api-spec.md). Not populated,
  // so normal knob browsing skips it until setCardText() fills it in.
  { "-- empty slot --\n\nnothing here\nyet", nullptr, 0, 0, ANIM_BOUNCE, false, ANIMATION_DURATION_MS },
   
  { "good night", nullptr, 0, 0, ANIM_MOON_SPARKLE, true, ANIMATION_DURATION_MS },
};

const uint8_t NUM_CARDS = sizeof(cards) / sizeof(cards[0]);

const Card& getCardContent(int index) {
  int wrapped = ((index % NUM_CARDS) + NUM_CARDS) % NUM_CARDS;
  return cards[wrapped];
}

// The default Adafruit_GFX font can't render real Unicode emoji at all
// (multi-byte UTF-8 just comes out as garbled box-drawing characters) -
// but it does include a handful of IBM CP437 "symbol" glyphs (hearts,
// suits, musical notes, smileys) as single bytes. The site should send
// these shortcodes rather than raw emoji. See docs/remote-api-spec.md for
// the supported list. Shared by applyEmojiShortcodes() (inline in card
// text) and emojiShortcodeToGlyph() (corner-emoji decorations).
static const struct { const char* code; char glyph; } EMOJI_SHORTCODES[] = {
  { ":heart:", (char)0x03 },
  { ":smile:", (char)0x01 },
  { ":smileb:", (char)0x02 },
  { ":sparkle:", (char)0x0F },
  { ":diamond:", (char)0x04 },
  { ":note:", (char)0x0D },
  { ":notes:", (char)0x0E },
  // No glyph shaped like the star bitmap exists in CP437 (this font's
  // only symbol set) - 0x07 (a filled circle/bullet) is just a safe
  // stand-in byte that a normal typed message would never produce by
  // accident; the actual rendering is the star bitmap, not this glyph.
  { ":star:", (char)0x07 },
};
static const int NUM_EMOJI_SHORTCODES = sizeof(EMOJI_SHORTCODES) / sizeof(EMOJI_SHORTCODES[0]);

char emojiShortcodeToGlyph(const char* code) {
  if (code == nullptr || code[0] == '\0') return 0;
  for (int i = 0; i < NUM_EMOJI_SHORTCODES; i++) {
    if (strcmp(code, EMOJI_SHORTCODES[i].code) == 0) return EMOJI_SHORTCODES[i].glyph;
  }
  return 0;
}

const char* glyphToEmojiShortcode(char glyph) {
  if (glyph == 0) return "";
  for (int i = 0; i < NUM_EMOJI_SHORTCODES; i++) {
    if (EMOJI_SHORTCODES[i].glyph == glyph) return EMOJI_SHORTCODES[i].code;
  }
  return "";
}

EmojiAnimFamily glyphToEmojiAnimFamily(char glyph) {
  switch ((uint8_t)glyph) {
    case 0x03: return EMOJI_ANIM_HEART;
    case 0x01: case 0x02: return EMOJI_ANIM_SMILE;
    case 0x0F: return EMOJI_ANIM_SPARKLE;
    case 0x04: return EMOJI_ANIM_DIAMOND;
    case 0x0D: case 0x0E: return EMOJI_ANIM_NOTES;
    case 0x07: return EMOJI_ANIM_STAR;
    default: return EMOJI_ANIM_NONE;
  }
}

static void applyEmojiShortcodes(char* text) {
  const auto& table = EMOJI_SHORTCODES;
  const int numCodes = NUM_EMOJI_SHORTCODES;

  char* src = text;
  char* dst = text;
  while (*src) {
    bool replaced = false;
    if (*src == ':') {
      for (int i = 0; i < numCodes; i++) {
        size_t len = strlen(table[i].code);
        if (strncmp(src, table[i].code, len) == 0) {
          *dst++ = table[i].glyph;
          src += len;
          replaced = true;
          break;
        }
      }
    }
    if (!replaced) {
      *dst++ = *src++;
    }
  }
  *dst = '\0';
}

void setCardText(int index, const char* text) {
  if (text == nullptr) return;
  int wrapped = ((index % NUM_CARDS) + NUM_CARDS) % NUM_CARDS;
  strncpy(cards[wrapped].text, text, CARD_TEXT_LEN - 1);
  cards[wrapped].text[CARD_TEXT_LEN - 1] = '\0';
  applyEmojiShortcodes(cards[wrapped].text);
  cards[wrapped].populated = true;
}

void setCardAnimationDuration(int index, uint16_t durationMs) {
  int wrapped = ((index % NUM_CARDS) + NUM_CARDS) % NUM_CARDS;
  cards[wrapped].animationDurationMs = durationMs;
}

void setCardAlignment(int index, TextAlignH alignH, TextAlignV alignV) {
  int wrapped = ((index % NUM_CARDS) + NUM_CARDS) % NUM_CARDS;
  cards[wrapped].alignH = alignH;
  cards[wrapped].alignV = alignV;
}

void setCardCornerEmoji(int index, int corner, char glyph) {
  if (corner < 0 || corner > 3) return;
  int wrapped = ((index % NUM_CARDS) + NUM_CARDS) % NUM_CARDS;
  cards[wrapped].cornerEmoji[corner] = glyph;
}

void setCardAnimatedEmojiDisableMask(int index, uint8_t disableMask) {
  int wrapped = ((index % NUM_CARDS) + NUM_CARDS) % NUM_CARDS;
  cards[wrapped].animatedEmojiDisableMask = disableMask;
}

static bool isCardVisible(int index) {
  const Card& c = cards[index];
  if (!c.populated) return false;
  // The clock card needs a real synced time to be meaningful - hide it
  // entirely until syncTime() has actually succeeded once this boot.
  if (c.animation == ANIM_CLOCK && !isTimeSynced()) return false;
  return true;
}

int nextVisibleIndex(int from, int direction) {
  int idx = from;
  for (int i = 0; i < NUM_CARDS; i++) {
    idx = ((idx + direction) % NUM_CARDS + NUM_CARDS) % NUM_CARDS;
    if (isCardVisible(idx)) return idx;
  }
  return from;  // defensive - unreachable since content cards are always populated
}
