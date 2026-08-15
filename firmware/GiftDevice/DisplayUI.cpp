#include <Arduino.h>
#include <Wire.h>
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

static void drawCardFrame(int index, bool sparkleOn) {
  const Card& card = getCardContent(index);

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println(card.text);

  if (card.bitmap != nullptr) {
    int x = OLED_WIDTH - card.bmpW - 2;
    int y = 2;
    display.drawBitmap(x, y, card.bitmap, card.bmpW, card.bmpH, SSD1306_WHITE);
  }

  // Card position indicator, bottom-right.
  display.setCursor(OLED_WIDTH - 30, OLED_HEIGHT - 8);
  display.print(index + 1);
  display.print("/");
  display.print(NUM_CARDS);

  if (sparkleOn) {
    // Simple placeholder "sparkle": blinking corner marks. Replace with a
    // nicer bitmap/animation once you decide what it should look like.
    display.setCursor(0, OLED_HEIGHT - 8);
    display.print("*");
    display.setCursor(OLED_WIDTH - 8, 0);
    display.print("*");
  }

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

void showCard(int index) {
  currentlyShownCard = index;
  animationUntilMs = 0;  // switching cards cancels any in-flight animation
  drawCardFrame(currentlyShownCard, false);
}

void triggerAnimation() {
  animationUntilMs = millis() + ANIMATION_DURATION_MS;
}

void updateDisplayAnimation() {
  if (animationUntilMs == 0) return;

  unsigned long now = millis();
  if (now >= animationUntilMs) {
    animationUntilMs = 0;
    drawCardFrame(currentlyShownCard, false);
    return;
  }

  // Blink the sparkle at ~5Hz while the animation window is open.
  bool sparkleOn = ((now / 100) % 2) == 0;
  drawCardFrame(currentlyShownCard, sparkleOn);
}
