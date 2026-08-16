#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "DisplayUI.h"
#include "Cards.h"
#include "Config.h"

// NOTE: if the 1.3" OLED turns out to use a different controller chip than
// SSD1306 once it arrives, swap this library for U8g2 - see plan section 8.
static Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

static int currentlyShownCard = 0;
static unsigned long animationUntilMs = 0;
static unsigned long animationDurationMs = ANIMATION_DURATION_MS;

// ---- shared drawing helpers ----

// Splits text on '\n', horizontally centers each line, and vertically
// centers the whole block within [yTop, yBottom).
static void printCentered(const char* text, int yTop, int yBottom, int lineHeight) {
  int numLines = 1;
  for (const char* p = text; *p; p++) {
    if (*p == '\n') numLines++;
  }

  int blockHeight = numLines * lineHeight;
  int y = yTop + ((yBottom - yTop) - blockHeight) / 2;
  if (y < yTop) y = yTop;

  const char* lineStart = text;

  while (*lineStart) {
    const char* nl = strchr(lineStart, '\n');
    int len = nl ? (int)(nl - lineStart) : (int)strlen(lineStart);

    char lineBuf[48];
    int copyLen = len < (int)sizeof(lineBuf) - 1 ? len : (int)sizeof(lineBuf) - 1;
    memcpy(lineBuf, lineStart, copyLen);
    lineBuf[copyLen] = '\0';

    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(lineBuf, 0, y, &x1, &y1, &w, &h);
    int x = (OLED_WIDTH - (int)w) / 2;
    if (x < 0) x = 0;
    display.setCursor(x, y);
    display.print(lineBuf);

    y += lineHeight;
    if (!nl) break;
    lineStart = nl + 1;
  }
}

// Small vector heart used both by the dedicated "i miss you" card
// (drawHeart() below) and by the animated heart glyph (drawAnimatedGlyph()
// right below) - moved up here so both can share it. Two filled circles
// for the lobes, one triangle for the point.
static void drawMiniHeart(int cx, int cy, int r) {
  display.fillCircle(cx - r / 2, cy - r / 3, r / 2, SSD1306_WHITE);
  display.fillCircle(cx + r / 2, cy - r / 3, r / 2, SSD1306_WHITE);
  display.fillTriangle(cx - r, cy - r / 4, cx + r, cy - r / 4, cx, cy + r, SSD1306_WHITE);
}

// Renders one emoji glyph's default animation in place of a plain print()
// - only called for glyphs whose family is animate-enabled on this card
// (see Card::animatedEmojiDisableMask) while a press-animation is running.
// x/y is the same cursor position a plain display.print() would have
// used; textSize matches whatever the caller drew everything else at (1
// for inline text, 2 for corner decorations) so this scales with it.
// Each case is deliberately simple - a 5x7 (or 10x14 at 2x) glyph cell
// doesn't have room for anything elaborate, just a clear "this one's
// animating" cue timed against the card's overall animation progress
// (0-1 over the full press-animation duration).
static void drawAnimatedGlyph(EmojiAnimFamily family, int x, int y, float progress, uint8_t textSize) {
  display.setTextSize(textSize);
  switch (family) {
    case EMOJI_ANIM_HEART: {
      // Real heartbeat (size pulse), same as the dedicated "i miss you"
      // card's drawHeart() - reuses its drawMiniHeart() vector shape
      // instead of an on/off blink of the CP437 heart glyph, which read
      // as blinking, not beating.
      int cx = x + 3 * textSize;
      int cy = y + 3 * textSize;
      float wave = fabsf(sinf(progress * PI * 3.0f));
      int r = (int)(2 * textSize * (1.0f + wave * 0.6f));
      if (r < 1) r = 1;
      drawMiniHeart(cx, cy, r);
      break;
    }
    case EMOJI_ANIM_SMILE: {
      // Alternates the outline (0x01) and filled (0x02) glyphs - reads
      // as cycling between a small smile and a full grin.
      char g = (fmodf(progress * 6.0f, 1.0f) < 0.5f) ? (char)0x01 : (char)0x02;
      display.setCursor(x, y);
      display.print(g);
      break;
    }
    case EMOJI_ANIM_DIAMOND: {
      // Brief size-flash instead of stray corner pixels - those read as
      // random dots, especially when diamond is placed as a corner
      // decoration itself (stray dots right next to the real screen
      // corner looked like a rendering glitch rather than an animation).
      float p = fmodf(progress * 4.0f, 1.0f);
      uint8_t flashSize = (p < 0.2f) ? (uint8_t)(textSize + 1) : textSize;
      display.setTextSize(flashSize);
      display.setCursor(x, y);
      display.print((char)0x04);
      break;
    }
    case EMOJI_ANIM_NOTES: {
      // Cycles single<->double note with a 1px bob - the closest a fixed
      // character cell can get to "spinning."
      float p = fmodf(progress * 5.0f, 1.0f);
      char g = (p < 0.5f) ? (char)0x0D : (char)0x0E;
      int yOff = (p < 0.25f || (p >= 0.5f && p < 0.75f)) ? 0 : -1 * (int)textSize;
      display.setCursor(x, y + yOff);
      display.print(g);
      break;
    }
    default:
      // EMOJI_ANIM_SPARKLE and EMOJI_ANIM_SNOWFLAKE aren't handled here -
      // CP437 doesn't have glyphs that actually read as either shape, so
      // both are drawn as real vector shapes instead (drawSparkleGlyph()/
      // drawSnowflakeGlyph() below), called directly from printAligned()/
      // drawCornerEmoji() rather than through this glyph-swap dispatcher.
      break;
  }
  display.setTextSize(1);
}

// Sparkle: reuses the same 4-point cross already used on the good-night
// card (drawSparkle() below) rather than a CP437 glyph - the closest
// available CP437 character is a sun-with-rays, which reads as a sun, not
// a sparkle. "Shine" is a radius pulse. x/y is the same top-left cursor
// position a plain glyph print would have used.
static void drawSparkleGlyph(int x, int y, uint8_t textSize, bool animating, float progress) {
  int cx = x + 2 * textSize;
  int cy = y + 3 * textSize;
  int r = 3 * textSize;
  if (animating) {
    float pulse = 0.5f + 0.5f * fabsf(sinf(progress * PI * 4.0f));
    r = (int)(r * pulse);
    if (r < 1) r = 1;
  }
  display.drawLine(cx - r, cy, cx + r, cy, SSD1306_WHITE);
  display.drawLine(cx, cy - r, cx, cy + r, SSD1306_WHITE);
}

// Snowflake: the exact logo_bmp bitmap from the Adafruit_SSD1306 example
// sketch's falling-icon demo
// (firmware/sanity-checks/display_hello_world/display_hello_world.ino,
// testanimate()), reused pixel-for-pixel rather than a CP437 glyph or a
// hand-drawn vector shape - it's the actual Adafruit logo silhouette, not
// a geometric snowflake, but it's the specific bitmap that was asked for.
static const unsigned char PROGMEM SNOWFLAKE_BMP[] = {
  0b00000000, 0b11000000,
  0b00000001, 0b11000000,
  0b00000001, 0b11000000,
  0b00000011, 0b11100000,
  0b11110011, 0b11100000,
  0b11111110, 0b11111000,
  0b01111110, 0b11111111,
  0b00110011, 0b10011111,
  0b00011111, 0b11111100,
  0b00001101, 0b01110000,
  0b00011011, 0b10100000,
  0b00111111, 0b11100000,
  0b00111111, 0b11110000,
  0b01111100, 0b11110000,
  0b01110000, 0b01110000,
  0b00000000, 0b00110000
};
#define SNOWFLAKE_BMP_W 16
#define SNOWFLAKE_BMP_H 16

// No true rotation is possible for a fixed bitmap (Adafruit_GFX doesn't
// support rotating drawBitmap()), so "spin" orbits its position slightly
// instead - same trick as the retired glyph-based version, still reads
// as "in motion" rather than static.
static void drawSnowflakeGlyph(int x, int y, uint8_t textSize, bool animating, float progress) {
  int ox = 0, oy = 0;
  if (animating) {
    float p = fmodf(progress * 3.0f, 1.0f);
    int step = (int)(p * 4.0f) % 4;
    static const int8_t dx[4] = { 0, 1, 0, -1 };
    static const int8_t dy[4] = { -1, 0, 1, 0 };
    ox = dx[step] * (int)textSize;
    oy = dy[step] * (int)textSize;
  }
  display.drawBitmap(x + ox, y + oy, SNOWFLAKE_BMP, SNOWFLAKE_BMP_W, SNOWFLAKE_BMP_H, SSD1306_WHITE);
}

// Generalized version of printCentered with per-axis alignment, used only
// by drawBounce() (the ANIM_BOUNCE / custom-card layout) - every other
// animation below calls printCentered/printLeftAligned directly with its
// own hand-tuned bounds, since a user-editable layout doesn't make sense
// on a fixed hand-drawn animation. Draws character-by-character (rather
// than one display.print(lineBuf) call per line) so animated emoji
// glyphs can be swapped out for drawAnimatedGlyph() at the exact position
// they'd otherwise print at - relies on the default font's fixed 6px
// (at size 1) advance per character to compute those positions without
// needing a getTextBounds() call per character.
static void printAligned(const char* text, TextAlignH alignH, TextAlignV alignV,
                          int xLeft, int xRight, int yTop, int yBottom, int lineHeight,
                          bool animating, float progress, uint8_t animDisableMask) {
  int numLines = 1;
  for (const char* p = text; *p; p++) {
    if (*p == '\n') numLines++;
  }
  int blockHeight = numLines * lineHeight;

  int y;
  if (alignV == ALIGN_V_TOP) {
    y = yTop;
  } else if (alignV == ALIGN_V_BOTTOM) {
    y = yBottom - blockHeight;
    if (y < yTop) y = yTop;
  } else {
    y = yTop + ((yBottom - yTop) - blockHeight) / 2;
    if (y < yTop) y = yTop;
  }

  const char* lineStart = text;
  while (*lineStart) {
    const char* nl = strchr(lineStart, '\n');
    int len = nl ? (int)(nl - lineStart) : (int)strlen(lineStart);

    char lineBuf[48];
    int copyLen = len < (int)sizeof(lineBuf) - 1 ? len : (int)sizeof(lineBuf) - 1;
    memcpy(lineBuf, lineStart, copyLen);
    lineBuf[copyLen] = '\0';

    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(lineBuf, 0, y, &x1, &y1, &w, &h);

    int x;
    if (alignH == ALIGN_H_LEFT) {
      x = xLeft;
    } else if (alignH == ALIGN_H_RIGHT) {
      x = xRight - (int)w;
      if (x < xLeft) x = xLeft;
    } else {
      x = xLeft + ((xRight - xLeft) - (int)w) / 2;
      if (x < xLeft) x = xLeft;
    }

    int cx = x;
    for (int i = 0; i < copyLen; i++) {
      char c = lineBuf[i];
      EmojiAnimFamily family = glyphToEmojiAnimFamily(c);
      bool animateThis = animating && family != EMOJI_ANIM_NONE &&
                          !(animDisableMask & (1 << family));
      if (family == EMOJI_ANIM_SPARKLE) {
        drawSparkleGlyph(cx, y, 1, animateThis, progress);
        cx += 6;
      } else if (family == EMOJI_ANIM_SNOWFLAKE) {
        // 16px-wide bitmap, not a 6px text-cell glyph - needs a wider
        // advance so the next character doesn't land on top of it.
        drawSnowflakeGlyph(cx, y, 1, animateThis, progress);
        cx += SNOWFLAKE_BMP_W;
      } else if (animateThis) {
        drawAnimatedGlyph(family, cx, y, progress, 1);
        cx += 6;
      } else {
        display.setCursor(cx, y);
        display.print(c);
        cx += 6;
      }
    }

    y += lineHeight;
    if (!nl) break;
    lineStart = nl + 1;
  }
}

// Draws the 4 corner-decoration glyphs (Card.cornerEmoji), skipping any
// that are 0 (unset). Inset just enough to clear the card's frame border.
// Drawn at 2x text size (Adafruit_GFX only offers integer multiples) -
// at the default size these single CP437 glyphs are ~5x7px and read as
// little more than a dot on a 128x64 screen.
static void drawCornerEmoji(const char cornerEmoji[4], bool animating, float progress, uint8_t animDisableMask) {
  // Symmetric 3px inset on every side - a 2x-size glyph cell is 10x14px,
  // so the right/bottom corners subtract cell size + inset from the edge.
  static const int cx[4] = { 3, OLED_WIDTH - 13, 3, OLED_WIDTH - 13 };
  static const int cy[4] = { 3, 3, OLED_HEIGHT - 17, OLED_HEIGHT - 17 };
  // Snowflake is a 16x16 bitmap, bigger than the other corner glyphs'
  // 10x14 cell - same symmetric 3px inset, but sized for its own bounds
  // so it doesn't run past the right/bottom edge.
  static const int snowCx[4] = { 3, OLED_WIDTH - 19, 3, OLED_WIDTH - 19 };
  static const int snowCy[4] = { 3, 3, OLED_HEIGHT - 19, OLED_HEIGHT - 19 };
  for (int i = 0; i < 4; i++) {
    if (cornerEmoji[i] == 0) continue;
    EmojiAnimFamily family = glyphToEmojiAnimFamily(cornerEmoji[i]);
    bool animateThis = animating && family != EMOJI_ANIM_NONE &&
                        !(animDisableMask & (1 << family));
    if (family == EMOJI_ANIM_SPARKLE) {
      drawSparkleGlyph(cx[i], cy[i], 2, animateThis, progress);
    } else if (family == EMOJI_ANIM_SNOWFLAKE) {
      drawSnowflakeGlyph(snowCx[i], snowCy[i], 2, animateThis, progress);
    } else if (animateThis) {
      drawAnimatedGlyph(family, cx[i], cy[i], progress, 2);
    } else {
      display.setTextSize(2);
      display.setCursor(cx[i], cy[i]);
      display.print(cornerEmoji[i]);
      display.setTextSize(1);
    }
  }
}

// Same as printCentered but left-aligned at a fixed x - used for cards
// with a side-by-side layout instead of caption-on-top.
static void printLeftAligned(const char* text, int x, int yTop, int yBottom, int lineHeight) {
  int numLines = 1;
  for (const char* p = text; *p; p++) {
    if (*p == '\n') numLines++;
  }

  int blockHeight = numLines * lineHeight;
  int y = yTop + ((yBottom - yTop) - blockHeight) / 2;
  if (y < yTop) y = yTop;

  const char* lineStart = text;

  while (*lineStart) {
    const char* nl = strchr(lineStart, '\n');
    int len = nl ? (int)(nl - lineStart) : (int)strlen(lineStart);

    char lineBuf[48];
    int copyLen = len < (int)sizeof(lineBuf) - 1 ? len : (int)sizeof(lineBuf) - 1;
    memcpy(lineBuf, lineStart, copyLen);
    lineBuf[copyLen] = '\0';

    display.setCursor(x, y);
    display.print(lineBuf);

    y += lineHeight;
    if (!nl) break;
    lineStart = nl + 1;
  }
}

// Small four-point sparkle, like a doodle version of a Discord-style
// sparkle emoji - reused on the good-night card.
static void drawSparkle(int x, int y, int r) {
  display.drawLine(x - r, y, x + r, y, SSD1306_WHITE);
  display.drawLine(x, y - r, x, y + r, SSD1306_WHITE);
}

// ---- per-card animations ----
// Each takes the card (for its caption text), whether the short-press
// animation is currently playing, and progress 0..1 through that window.

// A real enclosed corridor (outer wall + two partition walls, each with a
// gap on alternating ends) instead of floating dashes - Pac-Man's path
// below winds through it: right, down, left, down, right, up.
static void drawPacmanMaze() {
  display.drawRect(8, 8, 112, 50, SSD1306_WHITE);       // outer wall
  display.drawLine(8, 25, 100, 25, SSD1306_WHITE);      // partition 1, gap on the right (100-120)
  display.drawLine(28, 41, 120, 41, SSD1306_WHITE);     // partition 2, gap on the left (8-28)
}

struct PacPoint { int x, y; };
static const PacPoint PAC_PATH[] = {
  {14, 16}, {108, 16}, {108, 33}, {20, 33}, {20, 49}, {112, 49}, {112, 44}
};
#define PAC_PATH_LEN (sizeof(PAC_PATH) / sizeof(PAC_PATH[0]))

static void pacPositionAt(float progress, int& outX, int& outY, int& outDirX, int& outDirY) {
  float segLen[PAC_PATH_LEN - 1];
  float total = 0;
  for (unsigned i = 0; i < PAC_PATH_LEN - 1; i++) {
    float dx = (float)(PAC_PATH[i + 1].x - PAC_PATH[i].x);
    float dy = (float)(PAC_PATH[i + 1].y - PAC_PATH[i].y);
    segLen[i] = sqrtf(dx * dx + dy * dy);
    total += segLen[i];
  }

  float target = progress * total;
  float acc = 0;
  for (unsigned i = 0; i < PAC_PATH_LEN - 1; i++) {
    bool isLast = (i == PAC_PATH_LEN - 2);
    if (target <= acc + segLen[i] || isLast) {
      float t = segLen[i] > 0 ? (target - acc) / segLen[i] : 0;
      if (t < 0) t = 0;
      if (t > 1) t = 1;
      outX = PAC_PATH[i].x + (int)((PAC_PATH[i + 1].x - PAC_PATH[i].x) * t);
      outY = PAC_PATH[i].y + (int)((PAC_PATH[i + 1].y - PAC_PATH[i].y) * t);
      outDirX = (PAC_PATH[i + 1].x > PAC_PATH[i].x) ? 1 : (PAC_PATH[i + 1].x < PAC_PATH[i].x ? -1 : 0);
      outDirY = (PAC_PATH[i + 1].y > PAC_PATH[i].y) ? 1 : (PAC_PATH[i + 1].y < PAC_PATH[i].y ? -1 : 0);
      return;
    }
    acc += segLen[i];
  }
  outX = PAC_PATH[PAC_PATH_LEN - 1].x;
  outY = PAC_PATH[PAC_PATH_LEN - 1].y;
  outDirX = 0;
  outDirY = 0;
}

// Mouth wedge faces whichever way Pac-Man is currently moving, like real
// arrow-key gameplay rather than a fixed facing.
static void drawPacBody(int cx, int cy, int dirX, int dirY, bool mouthOpen) {
  display.fillCircle(cx, cy, 7, SSD1306_WHITE);
  if (!mouthOpen) return;

  if (dirY > 0) {
    display.fillTriangle(cx, cy, cx - 5, cy + 9, cx + 5, cy + 9, SSD1306_BLACK);
  } else if (dirY < 0) {
    display.fillTriangle(cx, cy, cx - 5, cy - 9, cx + 5, cy - 9, SSD1306_BLACK);
  } else if (dirX < 0) {
    display.fillTriangle(cx, cy, cx - 9, cy - 5, cx - 9, cy + 5, SSD1306_BLACK);
  } else {
    display.fillTriangle(cx, cy, cx + 9, cy - 5, cx + 9, cy + 5, SSD1306_BLACK);
  }
}

static void drawPacman(const Card& card, bool animating, float progress) {
  drawPacmanMaze();

  // Dots placed along the corridor, each tagged with roughly how far
  // along the path (0..1) Pac-Man needs to be before it's "eaten."
  struct PacDot { int x, y; float frac; };
  static const PacDot dots[] = {
    {30, 16, 0.05f}, {50, 16, 0.12f}, {70, 16, 0.18f}, {90, 16, 0.24f},
    {90, 33, 0.41f}, {70, 33, 0.48f}, {50, 33, 0.54f}, {30, 33, 0.61f},
    {40, 49, 0.75f}, {60, 49, 0.82f}, {80, 49, 0.88f}, {100, 49, 0.95f},
  };
  for (unsigned i = 0; i < sizeof(dots) / sizeof(dots[0]); i++) {
    bool eaten = animating && progress > dots[i].frac + 0.02f;
    if (!eaten) display.fillCircle(dots[i].x, dots[i].y, 2, SSD1306_WHITE);
  }

  // Ghost waits near the lane-1 exit - Pac-Man passes close by before
  // diving down through the gap to escape into the next corridor.
  int ghostX = 114, ghostY = 16;
  display.fillCircle(ghostX, ghostY - 3, 6, SSD1306_WHITE);
  display.fillRect(ghostX - 6, ghostY - 3, 12, 8, SSD1306_WHITE);
  display.fillCircle(ghostX - 2, ghostY - 4, 1, SSD1306_BLACK);
  display.fillCircle(ghostX + 2, ghostY - 4, 1, SSD1306_BLACK);

  int pacX, pacY, dirX, dirY;
  if (animating) {
    pacPositionAt(progress, pacX, pacY, dirX, dirY);
  } else {
    pacX = PAC_PATH[0].x;
    pacY = PAC_PATH[0].y;
    dirX = 1;
    dirY = 0;
  }
  bool mouthOpen = !animating || (((int)(progress * 14)) % 2 == 0);
  drawPacBody(pacX, pacY, dirX, dirY, mouthOpen);
}

// One bloom: pointed petals radiating from a center, plus a stem down to
// a shared base point (so two of these read as a tied bouquet).
static void drawFlowerBloom(int cx, int cy, int r, int stemBottomY) {
  for (int i = 0; i < 6; i++) {
    float angle = i * (2.0f * PI / 6.0f);
    int tipX = cx + (int)(cosf(angle) * r);
    int tipY = cy + (int)(sinf(angle) * r);
    float perp = angle + PI / 2.0f;
    int base1X = cx + (int)(cosf(perp) * (r / 4));
    int base1Y = cy + (int)(sinf(perp) * (r / 4));
    int base2X = cx - (int)(cosf(perp) * (r / 4));
    int base2Y = cy - (int)(sinf(perp) * (r / 4));
    display.fillTriangle(base1X, base1Y, base2X, base2Y, tipX, tipY, SSD1306_WHITE);
  }
  display.fillCircle(cx, cy, r / 3 + 1, SSD1306_BLACK);
  display.drawCircle(cx, cy, r / 3 + 1, SSD1306_WHITE);
  display.drawLine(cx, cy + r, cx, stemBottomY, SSD1306_WHITE);
}

static void drawButterfly(int x, int y, bool wingsUp) {
  int span = 5;
  if (wingsUp) {
    display.fillTriangle(x, y, x - span, y - 4, x - 1, y + 1, SSD1306_WHITE);
    display.fillTriangle(x, y, x + span, y - 4, x + 1, y + 1, SSD1306_WHITE);
  } else {
    display.fillTriangle(x, y, x - span, y + 4, x - 1, y - 1, SSD1306_WHITE);
    display.fillTriangle(x, y, x + span, y + 4, x + 1, y - 1, SSD1306_WHITE);
  }
  display.drawLine(x, y - 3, x, y + 3, SSD1306_WHITE);
}

static void drawFlower(const Card& card, bool animating, float progress) {
  printLeftAligned(card.text, 6, 6, OLED_HEIGHT - 6, 10);

  // Bouquet stays static during the animation - only the butterfly moves.
  int baseY = 56;
  drawFlowerBloom(90, 22, 9, baseY);
  drawFlowerBloom(104, 28, 7, baseY);

  // Leaves on the stems.
  display.fillTriangle(90, 38, 96, 34, 96, 42, SSD1306_WHITE);
  display.fillTriangle(104, 42, 98, 38, 98, 46, SSD1306_WHITE);

  // Ambient glitter around the bouquet, independent of the butterfly.
  int glitter[3][2] = {{70, 42}, {112, 20}, {116, 46}};
  for (int i = 0; i < 3; i++) {
    bool on = !animating || (((int)(progress * 8) + i) % 3 == 0);
    if (on) drawSparkle(glitter[i][0], glitter[i][1], 2);
  }

  int bx, by;
  bool wingsUp;
  if (animating) {
    float angle = progress * 2.0f * PI;
    bx = 74 + (int)(cosf(angle) * 8);
    by = 16 + (int)(sinf(angle) * 6);
    wingsUp = ((int)(progress * 12)) % 2 == 0;
  } else {
    bx = 78;
    by = 14;
    wingsUp = true;
  }
  drawButterfly(bx, by, wingsUp);
}

static void drawTerminal(const Card& card, bool animating, float progress) {
  // Little window title bar, like a real terminal app.
  display.drawLine(4, 10, OLED_WIDTH - 4, 10, SSD1306_WHITE);
  display.fillCircle(8, 6, 1, SSD1306_WHITE);
  display.fillCircle(13, 6, 1, SSD1306_WHITE);
  display.fillCircle(18, 6, 1, SSD1306_WHITE);

  printCentered(card.text, 13, OLED_HEIGHT - 4, 9);
  if (animating) {
    bool cursorOn = ((int)(progress * 6)) % 2 == 0;
    if (cursorOn) {
      display.setCursor(2, OLED_HEIGHT - 9);
      display.print("_");
    }
  }
}

// Same terminal window chrome, but the trailing run of '*' characters
// types itself in left-to-right on short press instead of a plain
// blinking cursor - untyped stars show as '_' placeholders (same glyph
// width as '*', so the centered text doesn't jitter as they fill in).
static void drawTerminalPassword(const Card& card, bool animating, float progress) {
  display.drawLine(4, 10, OLED_WIDTH - 4, 10, SSD1306_WHITE);
  display.fillCircle(8, 6, 1, SSD1306_WHITE);
  display.fillCircle(13, 6, 1, SSD1306_WHITE);
  display.fillCircle(18, 6, 1, SSD1306_WHITE);

  char buf[CARD_TEXT_LEN];
  strncpy(buf, card.text, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  if (animating) {
    int len = (int)strlen(buf);
    int starCount = 0;
    while (starCount < len && buf[len - 1 - starCount] == '*') starCount++;

    int starsShown = (int)(progress * (starCount + 1));
    if (starsShown > starCount) starsShown = starCount;

    for (int i = 0; i < starCount - starsShown; i++) {
      buf[len - 1 - i] = '_';
    }
  }

  printCentered(buf, 13, OLED_HEIGHT - 4, 9);
}

// Same terminal chrome, but types out `target` (which must appear
// verbatim somewhere in the card's text) left-to-right on short press,
// like the password effect but for any word/phrase instead of only a
// trailing run of '*'. The rest of the text shows normally throughout.
static void drawTerminalTypeReveal(const Card& card, bool animating, float progress, const char* target) {
  display.drawLine(4, 10, OLED_WIDTH - 4, 10, SSD1306_WHITE);
  display.fillCircle(8, 6, 1, SSD1306_WHITE);
  display.fillCircle(13, 6, 1, SSD1306_WHITE);
  display.fillCircle(18, 6, 1, SSD1306_WHITE);

  char buf[CARD_TEXT_LEN];
  strncpy(buf, card.text, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  if (animating) {
    char* found = strstr(buf, target);
    if (found != nullptr) {
      int targetLen = (int)strlen(target);
      int shown = (int)(progress * (targetLen + 1));
      if (shown > targetLen) shown = targetLen;
      for (int i = shown; i < targetLen; i++) {
        found[i] = '_';
      }
    }
  }

  printCentered(buf, 13, OLED_HEIGHT - 4, 9);
}

static void drawTerminalTypeStatus(const Card& card, bool animating, float progress) {
  drawTerminalTypeReveal(card, animating, progress, "200 OK");
}

static void drawTerminalTypeBugs(const Card& card, bool animating, float progress) {
  drawTerminalTypeReveal(card, animating, progress, "0.0");
}

static void drawTerminalTypeFirewall(const Card& card, bool animating, float progress) {
  drawTerminalTypeReveal(card, animating, progress, "my_heart");
}

static void drawHeart(const Card& card, bool animating, float progress) {
  printCentered(card.text, 8, 22, 10);

  int cx = 64, cy = 42;
  float scale = 1.0f;
  if (animating) {
    float wave = fabsf(sinf(progress * PI * 3.0f));
    scale = 1.0f + wave * 0.35f;
  }
  int rMain = (int)(8 * scale);
  int rMed = (int)(6 * scale);
  int rSmall = (int)(4 * scale);
  if (rSmall < 1) rSmall = 1;

  // Five hearts in one row, sized up toward the center - all pulse in
  // sync since they share the same scale each frame. Wider gaps around
  // the (bigger) center heart than between the outer pairs.
  drawMiniHeart(cx - 36, cy, rSmall);
  drawMiniHeart(cx - 20, cy, rMed);
  drawMiniHeart(cx, cy, rMain);
  drawMiniHeart(cx + 20, cy, rMed);
  drawMiniHeart(cx + 36, cy, rSmall);
}

static void drawBird(int x, int y) {
  display.drawLine(x - 3, y, x, y - 2, SSD1306_WHITE);
  display.drawLine(x, y - 2, x + 3, y, SSD1306_WHITE);
}

static void drawSunrise(const Card& card, bool animating, float progress) {
  printCentered(card.text, 6, 24, 10);

  // Clouds.
  display.fillCircle(22, 28, 4, SSD1306_WHITE);
  display.fillCircle(28, 26, 5, SSD1306_WHITE);
  display.fillCircle(34, 28, 4, SSD1306_WHITE);

  display.fillCircle(96, 24, 3, SSD1306_WHITE);
  display.fillCircle(101, 22, 4, SSD1306_WHITE);
  display.fillCircle(106, 24, 3, SSD1306_WHITE);

  // Birds drifting across the empty space below the clouds/sun, above the horizon.
  int birdShift = animating ? (int)(progress * 12) : 0;
  drawBird(46 + birdShift, 46);
  drawBird(58 + birdShift, 43);

  int cx = 64, horizonY = 48, r = 10;
  // Rise distance kept short (12, not 20) so the sun's rays never reach
  // as high as the caption text - a ray was previously landing right on
  // top of the "o" in "morning" and making it read as a "b".
  int sunY = animating ? (int)(horizonY - progress * 12) : horizonY - 12;

  for (int i = 0; i < 8; i++) {
    float angle = i * (2.0f * PI / 8.0f);
    int x1 = cx + (int)(cosf(angle) * (r + 3));
    int y1 = sunY + (int)(sinf(angle) * (r + 3));
    int x2 = cx + (int)(cosf(angle) * (r + 7));
    int y2 = sunY + (int)(sinf(angle) * (r + 7));
    if (y1 >= 22 && y2 <= horizonY + 6) {
      display.drawLine(x1, y1, x2, y2, SSD1306_WHITE);
    }
  }
  display.fillCircle(cx, sunY, r, SSD1306_WHITE);
}

static void drawMoonSparkle(const Card& card, bool animating, float progress) {
  printCentered(card.text, 6, 24, 10);

  int cx = 64, cy = 40, r = 12;
  display.fillCircle(cx, cy, r, SSD1306_WHITE);
  display.fillCircle(cx + 6, cy - 3, r - 2, SSD1306_BLACK);  // carve crescent

  int stars[8][2] = {{18, 14}, {104, 16}, {12, 50}, {114, 48}, {64, 58}, {30, 52}, {96, 54}, {48, 26}};
  for (int i = 0; i < 8; i++) {
    bool on = !animating || (((int)(progress * 10) + i * 2) % 4 < 2);
    if (on) drawSparkle(stars[i][0], stars[i][1], 2);
  }
}

static void drawTimeSwirl(const Card& card, bool animating, float progress) {
  printCentered(card.text, 6, 24, 9);

  int cx = 64, cy = 44;

  // Clock-face ring of tick marks around the hourglass.
  for (int i = 0; i < 12; i++) {
    float a = i * (2.0f * PI / 12.0f);
    int x1 = cx + (int)(cosf(a) * 12);
    int y1 = cy + (int)(sinf(a) * 12);
    int x2 = cx + (int)(cosf(a) * 15);
    int y2 = cy + (int)(sinf(a) * 15);
    display.drawLine(x1, y1, x2, y2, SSD1306_WHITE);
  }

  display.drawTriangle(cx - 8, cy - 10, cx + 8, cy - 10, cx, cy, SSD1306_WHITE);
  display.drawTriangle(cx - 8, cy + 10, cx + 8, cy + 10, cx, cy, SSD1306_WHITE);

  if (animating) {
    float angle = progress * 4.0f * PI;
    float radius = 16.0f - progress * 10.0f;
    for (int i = 0; i < 6; i++) {
      float a = angle + i * (2.0f * PI / 6.0f);
      int x = cx + (int)(cosf(a) * radius);
      int y = cy + (int)(sinf(a) * radius);
      display.drawPixel(x, y, SSD1306_WHITE);
    }
  }
}

static void drawBoxBurst(const Card& card, bool animating, float progress) {
  printCentered(card.text, 6, 24, 9);

  int cx = 64, cy = 44;
  int boxW = 20, boxH = 14;
  float lidOpen = 0.0f;
  bool burst = false;

  if (animating) {
    if (progress < 0.5f) {
      cx += (((int)(progress * 40)) % 2 == 0) ? 1 : -1;
    } else {
      lidOpen = (progress - 0.5f) * 2.0f;
      burst = true;
    }
  }

  display.drawRect(cx - boxW / 2, cy - boxH / 2, boxW, boxH, SSD1306_WHITE);
  display.drawLine(cx, cy - boxH / 2, cx, cy + boxH / 2, SSD1306_WHITE);
  display.drawLine(cx - boxW / 2, cy, cx + boxW / 2, cy, SSD1306_WHITE);

  int lidY = cy - boxH / 2 - (int)(lidOpen * 8);
  display.drawLine(cx - boxW / 2, cy - boxH / 2, cx + boxW / 2, lidY, SSD1306_WHITE);

  // Bow on top of the ribbon, so it actually reads as a wrapped box.
  int bowY = cy - boxH / 2 - (int)(lidOpen * 8);
  display.fillTriangle(cx, bowY, cx - 7, bowY - 7, cx - 2, bowY - 2, SSD1306_WHITE);
  display.fillTriangle(cx, bowY, cx + 7, bowY - 7, cx + 2, bowY - 2, SSD1306_WHITE);
  display.fillCircle(cx, bowY - 1, 2, SSD1306_WHITE);

  if (burst) {
    for (int i = 0; i < 8; i++) {
      float a = i * (2.0f * PI / 8.0f);
      int x = cx + (int)(cosf(a) * 20);
      int y = (cy - boxH / 2 - 6) + (int)(sinf(a) * 12);
      drawSparkle(x, y, 2);
    }
    int confetti[4][2] = {{cx - 26, cy - 8}, {cx + 26, cy - 4}, {cx - 22, cy + 11}, {cx + 24, cy + 10}};
    for (int i = 0; i < 4; i++) {
      display.fillRect(confetti[i][0], confetti[i][1], 2, 2, SSD1306_WHITE);
    }
  }
}

static void drawCounter(const Card& card, bool animating, float progress) {
  printCentered(card.text, 4, 16, 9);

  int barX = 14, barY = 27, barW = 100, barH = 6;
  display.setCursor(barX, barY - 9);
  display.print("uptime: 3yr");
  display.drawRect(barX, barY, barW, barH, SSD1306_WHITE);
  float fill = animating ? progress : 1.0f;
  display.fillRect(barX + 1, barY + 1, (int)((barW - 2) * fill), barH - 2, SSD1306_WHITE);

  display.setCursor(14, 41);
  display.print("downtime: 0");
  display.setCursor(14, 53);
  display.print("bugs found: 0");
}

static void drawConstellation(const Card& card, bool animating, float progress) {
  printCentered(card.text, 4, 24, 9);

  // Kept well below the caption so the dots don't run into the text.
  int pts[5][2] = {{30, 32}, {50, 33}, {75, 36}, {95, 30}, {60, 54}};
  for (int i = 0; i < 5; i++) {
    display.fillCircle(pts[i][0], pts[i][1], 2, SSD1306_WHITE);
  }

  int order[4][2] = {{0, 1}, {1, 2}, {2, 3}, {1, 4}};
  int segmentsToShow = animating ? (int)(progress * 4) + 1 : 4;
  if (segmentsToShow > 4) segmentsToShow = 4;

  for (int i = 0; i < segmentsToShow; i++) {
    int a = order[i][0], b = order[i][1];
    display.drawLine(pts[a][0], pts[a][1], pts[b][0], pts[b][1], SSD1306_WHITE);
  }

  // Ambient background twinkle, separate from the constellation itself.
  int bg[4][2] = {{16, 46}, {110, 44}, {22, 58}, {104, 56}};
  for (int i = 0; i < 4; i++) {
    bool on = !animating || (((int)(progress * 10) + i * 3) % 5 < 2);
    if (on) display.drawPixel(bg[i][0], bg[i][1], SSD1306_WHITE);
  }
}

static void drawMapPin(const Card& card, bool animating, float progress) {
  printCentered(card.text, 4, 24, 9);

  // Simplified India outline (hand-approximated, not to scale) - kept
  // below the caption for margin. The Northeast panhandle spike and the
  // sharp southern tip are what make this read as India rather than a
  // generic tapered blob - swap for a traced bitmap via image2cpp if you
  // want a more accurate one.
  int outline[][2] = {
    {58, 26}, {70, 28}, {78, 33}, {72, 37}, {68, 42},
    {65, 50}, {60, 60}, {54, 51}, {50, 42}, {46, 31},
    {52, 26}, {58, 26}
  };
  int n = sizeof(outline) / sizeof(outline[0]);
  for (int i = 0; i < n - 1; i++) {
    display.drawLine(outline[i][0], outline[i][1], outline[i + 1][0], outline[i + 1][1], SSD1306_WHITE);
  }

  int pinX = 60, pinY = 42;
  bool pinVisible = !animating || (((int)(progress * 8)) % 2 == 0);
  if (pinVisible) {
    display.fillCircle(pinX, pinY, 3, SSD1306_WHITE);
    display.drawLine(pinX, pinY + 3, pinX, pinY + 8, SSD1306_WHITE);
  }

  // Small compass rose accent, bottom-right corner.
  int compassX = 112, compassY = 50;
  display.drawLine(compassX, compassY - 5, compassX, compassY + 5, SSD1306_WHITE);
  display.drawLine(compassX - 5, compassY, compassX + 5, compassY, SSD1306_WHITE);
  display.setCursor(compassX - 2, compassY - 13);
  display.print("N");
}

static void drawDinoRun(const Card& card, bool animating, float progress) {
  printCentered(card.text, 4, 20, 8);

  // Ground line, like the actual Chrome dino game.
  int groundY = 56;
  display.drawLine(6, groundY, OLED_WIDTH - 6, groundY, SSD1306_WHITE);

  int dinoX = 26;
  int legTop = groundY - 6;
  int bodyTop = legTop - 14;
  int headTop = bodyTop - 10;

  display.fillRect(dinoX - 8, bodyTop, 16, 14, SSD1306_WHITE);
  display.fillRect(dinoX + 2, headTop, 10, 10, SSD1306_WHITE);
  display.fillCircle(dinoX + 9, headTop + 4, 1, SSD1306_BLACK);
  display.fillTriangle(dinoX - 8, bodyTop + 2, dinoX - 17, bodyTop + 8, dinoX - 8, legTop, SSD1306_WHITE);

  bool frameA = !animating || (((int)(progress * 10)) % 2 == 0);
  if (frameA) {
    display.fillRect(dinoX - 6, legTop, 4, 6, SSD1306_WHITE);
    display.fillRect(dinoX + 2, legTop, 4, 4, SSD1306_WHITE);
  } else {
    display.fillRect(dinoX - 6, legTop, 4, 4, SSD1306_WHITE);
    display.fillRect(dinoX + 2, legTop, 4, 6, SSD1306_WHITE);
  }

  // Cacti scroll in toward the dino while animating, like the real game -
  // the second one is a bigger variety than the first.
  int scrollOffset = animating ? (int)(progress * 40) : 0;
  int cactusStartX[2] = {70, 100};
  for (int i = 0; i < 2; i++) {
    int cxq = cactusStartX[i] - scrollOffset;
    if (cxq < dinoX + 16) continue;  // "collided" - out of view

    bool big = (i == 1);
    int stalkW = big ? 6 : 4;
    int stalkH = big ? 20 : 14;
    int armW = big ? 5 : 4;
    int armH = big ? 5 : 4;

    display.fillRect(cxq - stalkW / 2, groundY - stalkH, stalkW, stalkH, SSD1306_WHITE);
    display.fillRect(cxq - stalkW / 2 - armW, groundY - stalkH + 4, armW, armH, SSD1306_WHITE);
    display.fillRect(cxq + stalkW / 2, groundY - stalkH + 6, armW, armH, SSD1306_WHITE);
  }
}

static void drawPulseBox(const Card& card, bool animating, float progress) {
  printCentered(card.text, 6, OLED_HEIGHT - 4, 9);

  // Viewfinder-style corner brackets, always on - extra emphasis beyond
  // the shared frame (drawn once for every card in drawCardFrame()).
  int x1 = 6, y1 = 6, x2 = OLED_WIDTH - 6, y2 = OLED_HEIGHT - 6;
  display.drawLine(x1, y1, x1 + 6, y1, SSD1306_WHITE);
  display.drawLine(x1, y1, x1, y1 + 6, SSD1306_WHITE);
  display.drawLine(x2, y1, x2 - 6, y1, SSD1306_WHITE);
  display.drawLine(x2, y1, x2, y1 + 6, SSD1306_WHITE);
  display.drawLine(x1, y2, x1 + 6, y2, SSD1306_WHITE);
  display.drawLine(x1, y2, x1, y2 - 6, SSD1306_WHITE);
  display.drawLine(x2, y2, x2 - 6, y2, SSD1306_WHITE);
  display.drawLine(x2, y2, x2, y2 - 6, SSD1306_WHITE);

  // The pulsing halo breathes inward from the shared frame (which now
  // sits flush against the screen edge, so there's no room to grow
  // outward without clipping off-screen).
  if (animating) {
    float wave = fabsf(sinf(progress * PI * 2.0f));
    int inset = (int)(wave * 4);
    display.drawRect(inset, inset, OLED_WIDTH - inset * 2, OLED_HEIGHT - inset * 2, SSD1306_WHITE);
  }
}

static void drawPeekaboo(const Card& card, bool animating, float progress) {
  printCentered(card.text, 6, OLED_HEIGHT - 4, 10);
  if (!animating) return;

  if (progress > 0.15f && progress < 0.4f) {
    drawSparkle(16, 14, 3);
  }
  if (progress > 0.3f && progress < 0.7f) {
    int x = 106, y = 16;
    display.drawCircle(x, y, 6, SSD1306_WHITE);
    display.drawLine(x - 3, y - 1, x - 1, y - 1, SSD1306_WHITE);  // closed wink eye
    display.drawCircle(x + 2, y - 1, 1, SSD1306_WHITE);           // open eye
    display.drawLine(x - 2, y + 3, x + 3, y + 2, SSD1306_WHITE);  // smirk
  }
}

static void drawFlirt(const Card& card, bool animating, float progress) {
  printCentered(card.text, 6, 24, 10);

  int cx = 64, cy = 42;
  display.drawCircle(cx, cy, 14, SSD1306_WHITE);

  display.fillCircle(cx - 6, cy - 2, 2, SSD1306_WHITE);
  display.fillCircle(cx + 6, cy - 2, 2, SSD1306_WHITE);

  // right eyebrow raises and lowers on repeat - the classic "hey there" look
  int browLift = animating ? (int)(fabsf(sinf(progress * PI * 3.0f)) * 4) : 2;
  display.drawLine(cx + 2, cy - 8 - browLift, cx + 10, cy - 8 - browLift, SSD1306_WHITE);
  display.drawLine(cx - 10, cy - 8, cx - 2, cy - 8, SSD1306_WHITE);

  // smirk
  display.drawLine(cx - 6, cy + 7, cx + 2, cy + 8, SSD1306_WHITE);
  display.drawLine(cx + 2, cy + 8, cx + 7, cy + 4, SSD1306_WHITE);

  if (animating) {
    bool sparkleOn = ((int)(progress * 8)) % 2 == 0;
    if (sparkleOn) drawSparkle(cx + 22, cy - 12, 3);
    bool sparkle2On = ((int)(progress * 8) + 1) % 3 == 0;
    if (sparkle2On) drawSparkle(cx - 24, cy - 4, 2);

    int heartY = cy - 20 - (int)(progress * 10);
    drawMiniHeart(cx + 20, heartY, 3);
  }
}

static void drawDuckFlush(const Card& card, bool animating, float progress) {
  printCentered(card.text, 4, 30, 9);

  int bowlCx = 64, bowlCy = 52;
  int bowlW = 30, bowlH = 14;

  display.drawRoundRect(bowlCx - bowlW / 2, bowlCy - bowlH / 2, bowlW, bowlH, 6, SSD1306_WHITE);

  if (animating) {
    // Water swirling down the drain.
    float angle = progress * 6.0f * PI;
    float radius = 10.0f - progress * 8.0f;
    for (int i = 0; i < 3; i++) {
      float a = angle + i * (2.0f * PI / 3.0f);
      int x = bowlCx + (int)(cosf(a) * radius);
      int y = bowlCy + (int)(sinf(a) * radius * 0.5f);
      display.drawPixel(x, y, SSD1306_WHITE);
    }
  }

  // Duck bobs above the bowl normally; dips down into the flush and pops
  // back up by the end of the animation - it survives, per the caption.
  int restY = bowlCy - 13;
  int duckY = restY;
  if (animating) {
    float wave = fabsf(sinf(progress * PI));  // 0 -> 1 -> 0
    duckY = restY + (int)(wave * 12);
  }
  int duckX = bowlCx;

  display.fillCircle(duckX, duckY, 5, SSD1306_WHITE);
  display.fillCircle(duckX + 4, duckY - 4, 3, SSD1306_WHITE);
  display.fillTriangle(duckX + 6, duckY - 4, duckX + 10, duckY - 3, duckX + 6, duckY - 2, SSD1306_WHITE);
  display.fillCircle(duckX + 5, duckY - 5, 1, SSD1306_BLACK);
}

// No animation on this one by design - it's a live clock, not a card
// with a press effect. Live ticking (including seconds) is handled by
// updateDisplayAnimation() redrawing this card once per second while
// it's showing - see lastClockTimeStr below.
static void drawClock(const Card& card, bool animating, float progress) {
  struct tm timeinfo;
  bool synced = getLocalTime(&timeinfo, 10);

  char dateBuf[20] = "-- --- --";
  char timeBuf[10] = "--:--:--";
  if (synced) {
    strftime(dateBuf, sizeof(dateBuf), "%a, %b %d", &timeinfo);
    strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &timeinfo);
  }

  int16_t x1, y1;
  uint16_t w, h;

  display.setTextSize(1);
  display.getTextBounds(dateBuf, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((OLED_WIDTH - (int)w) / 2, 14);
  display.print(dateBuf);

  display.setTextSize(2);
  display.getTextBounds(timeBuf, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((OLED_WIDTH - (int)w) / 2, 32);
  display.print(timeBuf);
  display.setTextSize(1);
}

// No press animation by design - the bars move continuously on their
// own (driven by millis(), not `progress`), redrawn periodically by
// updateDisplayAnimation() the same way the clock ticks.
static void drawEqualizer(const Card& card, bool animating, float progress) {
  printCentered(card.text, 6, 24, 9);

  const int barCount = 10;
  const int barWidth = 6;
  const int spacing = 10;
  const int barBaseY = 56;
  int startX = 64 - (barCount * spacing) / 2 + (spacing - barWidth) / 2;

  unsigned long t = millis();
  for (int i = 0; i < barCount; i++) {
    // /0.75 stretches the period out, i.e. 0.75x the original speed.
    float phase = (t / (150.0f / 0.75f)) + i * 1.3f;
    float wave = sinf(phase) * 0.5f + 0.5f;  // 0..1
    int barHeight = 6 + (int)(wave * 26);
    display.fillRect(startX + i * spacing, barBaseY - barHeight, barWidth, barHeight, SSD1306_WHITE);
  }
}

// Continuous ambient "screensaver" - reuses the exact falling-icon
// mechanism from the Adafruit_SSD1306 example sketch's testanimate()
// (firmware/sanity-checks/display_hello_world/display_hello_world.ino),
// just redrawn incrementally each tick instead of that demo's blocking
// for(;;) loop, so it fits the same live-redraw pattern as the clock/
// equalizer cards below. animating/progress are intentionally unused -
// like those two, this ignores short-press entirely and just keeps
// running on its own (see updateDisplayAnimation()).
#define SNOWFALL_COUNT 6
static void drawSnowfall(const Card& card, bool animating, float progress) {
  (void)animating;
  (void)progress;

  static int8_t flakeX[SNOWFALL_COUNT];
  static int8_t flakeY[SNOWFALL_COUNT];
  static int8_t flakeDY[SNOWFALL_COUNT];
  static bool initialized = false;

  if (!initialized) {
    for (int f = 0; f < SNOWFALL_COUNT; f++) {
      flakeX[f] = random(1 - SNOWFLAKE_BMP_W, OLED_WIDTH);
      flakeY[f] = -SNOWFLAKE_BMP_H;
      flakeDY[f] = random(1, 4);
    }
    initialized = true;
  }

  printCentered(card.text, 2, 14, 9);

  for (int f = 0; f < SNOWFALL_COUNT; f++) {
    display.drawBitmap(flakeX[f], flakeY[f], SNOWFLAKE_BMP, SNOWFLAKE_BMP_W, SNOWFLAKE_BMP_H, SSD1306_WHITE);
  }

  for (int f = 0; f < SNOWFALL_COUNT; f++) {
    flakeY[f] += flakeDY[f];
    if (flakeY[f] >= OLED_HEIGHT) {
      flakeX[f] = random(1 - SNOWFLAKE_BMP_W, OLED_WIDTH);
      flakeY[f] = -SNOWFLAKE_BMP_H;
      flakeDY[f] = random(1, 4);
    }
  }
}

static void drawBounce(const Card& card, bool animating, float progress) {
  printAligned(card.text, card.alignH, card.alignV, 4, OLED_WIDTH - 4, 0, OLED_HEIGHT, 9,
               animating, progress, card.animatedEmojiDisableMask);
  if (animating && !(card.animatedEmojiDisableMask & (1 << BORDER_FLASH_DISABLE_BIT))) {
    // Border-flash: a ring inset from the permanent frame blinks on/off -
    // this layout's "something is happening" cue. The permanent outer
    // frame (drawn by drawCardFrame() after this returns) stays solid
    // throughout, so this reads as a second ring flashing just inside it.
    float phase = fmodf(progress * 6.0f, 1.0f);
    if (phase < 0.5f) {
      display.drawRect(2, 2, OLED_WIDTH - 4, OLED_HEIGHT - 4, SSD1306_WHITE);
    }
  }
}

static void drawCardFrame(int index, bool animating, float progress) {
  const Card& card = getCardContent(index);

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  switch (card.animation) {
    case ANIM_PACMAN:        drawPacman(card, animating, progress); break;
    case ANIM_FLOWER:        drawFlower(card, animating, progress); break;
    case ANIM_TERMINAL:      drawTerminal(card, animating, progress); break;
    case ANIM_TERMINAL_PASSWORD: drawTerminalPassword(card, animating, progress); break;
    case ANIM_TERMINAL_TYPE_STATUS: drawTerminalTypeStatus(card, animating, progress); break;
    case ANIM_TERMINAL_TYPE_BUGS: drawTerminalTypeBugs(card, animating, progress); break;
    case ANIM_TERMINAL_TYPE_FIREWALL: drawTerminalTypeFirewall(card, animating, progress); break;
    case ANIM_HEART:         drawHeart(card, animating, progress); break;
    case ANIM_SUNRISE:       drawSunrise(card, animating, progress); break;
    case ANIM_MOON_SPARKLE:  drawMoonSparkle(card, animating, progress); break;
    case ANIM_TIME_SWIRL:    drawTimeSwirl(card, animating, progress); break;
    case ANIM_BOX_BURST:    drawBoxBurst(card, animating, progress); break;
    case ANIM_COUNTER:       drawCounter(card, animating, progress); break;
    case ANIM_CONSTELLATION: drawConstellation(card, animating, progress); break;
    case ANIM_MAP_PIN:       drawMapPin(card, animating, progress); break;
    case ANIM_DINO_RUN:      drawDinoRun(card, animating, progress); break;
    case ANIM_PULSE_BOX:     drawPulseBox(card, animating, progress); break;
    case ANIM_PEEKABOO:      drawPeekaboo(card, animating, progress); break;
    case ANIM_FLIRT:         drawFlirt(card, animating, progress); break;
    case ANIM_CLOCK:         drawClock(card, animating, progress); break;
    case ANIM_EQUALIZER:     drawEqualizer(card, animating, progress); break;
    case ANIM_DUCK_FLUSH:    drawDuckFlush(card, animating, progress); break;
    case ANIM_SNOWFALL:      drawSnowfall(card, animating, progress); break;
    case ANIM_BOUNCE:
    default:                 drawBounce(card, animating, progress); break;
  }

  // Permanent frame around every card, regardless of animation state.
  display.drawRect(0, 0, OLED_WIDTH, OLED_HEIGHT, SSD1306_WHITE);

  if (card.bitmap != nullptr) {
    int x = OLED_WIDTH - card.bmpW - 2;
    int y = 2;
    display.drawBitmap(x, y, card.bitmap, card.bmpW, card.bmpH, SSD1306_WHITE);
  }

  drawCornerEmoji(card.cornerEmoji, animating, progress, card.animatedEmojiDisableMask);

  display.display();
}

void initDisplay() {
  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
    Serial.println("OLED init failed - check wiring/address.");
    return;
  }
  display.clearDisplay();
  display.display();
}

static char lastClockTimeStr[10] = "";
static unsigned long lastEqualizerRedrawMs = 0;
static unsigned long lastSnowfallRedrawMs = 0;

void showCard(int index) {
  currentlyShownCard = index;
  animationUntilMs = 0;  // switching cards cancels any in-flight animation
  lastClockTimeStr[0] = '\0';  // force the clock card to redraw fresh if we land on it
  drawCardFrame(currentlyShownCard, false, 0.0f);
}

void triggerAnimation() {
  const Card& card = getCardContent(currentlyShownCard);
  animationDurationMs = card.animationDurationMs > 0 ? card.animationDurationMs : ANIMATION_DURATION_MS;
  animationUntilMs = millis() + animationDurationMs;
}

void updateDisplayAnimation() {
  if (animationUntilMs != 0) {
    unsigned long now = millis();
    if (now >= animationUntilMs) {
      animationUntilMs = 0;
      drawCardFrame(currentlyShownCard, false, 0.0f);
      return;
    }

    unsigned long elapsed = animationDurationMs - (animationUntilMs - now);
    float progress = (float)elapsed / (float)animationDurationMs;
    drawCardFrame(currentlyShownCard, true, progress);
    return;
  }

  CardAnimation liveAnim = getCardContent(currentlyShownCard).animation;

  // Live tick for the clock card - nothing else redraws on its own once
  // an animation finishes, but a clock needs to keep moving.
  if (liveAnim == ANIM_CLOCK) {
    char buf[10] = "--:--:--";
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 10)) {
      strftime(buf, sizeof(buf), "%H:%M:%S", &timeinfo);
    }
    if (strcmp(buf, lastClockTimeStr) != 0) {
      strncpy(lastClockTimeStr, buf, sizeof(lastClockTimeStr) - 1);
      lastClockTimeStr[sizeof(lastClockTimeStr) - 1] = '\0';
      drawCardFrame(currentlyShownCard, false, 0.0f);
    }
  }

  // Live redraw for the equalizer card - the bars keep moving on their
  // own, not gated by a short-press window.
  if (liveAnim == ANIM_EQUALIZER) {
    unsigned long now = millis();
    if (now - lastEqualizerRedrawMs >= 120) {
      lastEqualizerRedrawMs = now;
      drawCardFrame(currentlyShownCard, false, 0.0f);
    }
  }

  // Live redraw for the snowfall screensaver card - flakes keep falling
  // on their own, same pattern as the equalizer bars above.
  if (liveAnim == ANIM_SNOWFALL) {
    unsigned long now = millis();
    if (now - lastSnowfallRedrawMs >= 150) {
      lastSnowfallRedrawMs = now;
      drawCardFrame(currentlyShownCard, false, 0.0f);
    }
  }
}

void showGamePlaceholder() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  printCentered("Game Mode\n\n(not built yet -\nlong-press to\ngo back)", 0, OLED_HEIGHT, 9);
  display.drawRect(0, 0, OLED_WIDTH, OLED_HEIGHT, SSD1306_WHITE);
  display.display();
}

static char lastNightTimeStr[8] = "";

void enterNightMode() {
  display.dim(true);
  lastNightTimeStr[0] = '\0';  // force a redraw on the next renderNightMode()
}

void exitNightMode() {
  display.dim(false);
}

void renderNightMode() {
  char buf[8] = "--:--";
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 10)) {
    strftime(buf, sizeof(buf), "%H:%M", &timeinfo);
  }
  if (strcmp(buf, lastNightTimeStr) == 0) return;  // nothing changed, skip the redraw
  strncpy(lastNightTimeStr, buf, sizeof(lastNightTimeStr) - 1);
  lastNightTimeStr[sizeof(lastNightTimeStr) - 1] = '\0';

  display.clearDisplay();
  display.fillScreen(SSD1306_WHITE);

  display.setTextColor(SSD1306_BLACK);
  display.setTextSize(3);
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
  int textX = (OLED_WIDTH - (int)w) / 2 - 10;
  int textY = (OLED_HEIGHT - (int)h) / 2;
  display.setCursor(textX, textY);
  display.print(buf);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Small crescent moon beside the time, cut out of the white background.
  int moonX = textX + (int)w + 14;
  int moonY = OLED_HEIGHT / 2;
  display.fillCircle(moonX, moonY, 7, SSD1306_BLACK);
  display.fillCircle(moonX + 3, moonY - 2, 6, SSD1306_WHITE);

  display.display();
}

void flashIdentify() {
  display.invertDisplay(true);
  delay(150);
  display.invertDisplay(false);
}

void showWifiSetupPrompt(const char* apName) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  char msg[96];
  snprintf(msg, sizeof(msg), "WiFi Setup\n\nOn your phone,\njoin WiFi:\n%s", apName);
  printCentered(msg, 4, OLED_HEIGHT - 4, 9);

  display.drawRect(0, 0, OLED_WIDTH, OLED_HEIGHT, SSD1306_WHITE);
  display.display();
}

void showWifiConnected(const char* ip) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  char msg[64];
  snprintf(msg, sizeof(msg), "WiFi connected!\n\n%s", ip);
  printCentered(msg, 4, OLED_HEIGHT - 4, 9);

  display.drawRect(0, 0, OLED_WIDTH, OLED_HEIGHT, SSD1306_WHITE);
  display.display();
  delay(2000);
}

void showWifiFailed() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  printCentered("WiFi setup failed\n\nContinuing\noffline", 4, OLED_HEIGHT - 4, 9);

  display.drawRect(0, 0, OLED_WIDTH, OLED_HEIGHT, SSD1306_WHITE);
  display.display();
  delay(2000);
}
