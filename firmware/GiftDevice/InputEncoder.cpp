#include <Arduino.h>
#include "InputEncoder.h"
#include "Config.h"

static int lastClkState = HIGH;
static unsigned long lastEncoderChangeMs = 0;

static bool buttonWasDown = false;
static unsigned long buttonDownAtMs = 0;

void initEncoder() {
  pinMode(PIN_ENC_CLK, INPUT_PULLUP);
  pinMode(PIN_ENC_DT, INPUT_PULLUP);
  pinMode(PIN_ENC_SW, INPUT_PULLUP);  // KY-040 button reads LOW when pressed
  lastClkState = digitalRead(PIN_ENC_CLK);
}

static EncoderEvent pollRotation() {
  int clk = digitalRead(PIN_ENC_CLK);
  unsigned long now = millis();

  if (clk != lastClkState && (now - lastEncoderChangeMs) > ENCODER_DEBOUNCE_MS) {
    lastEncoderChangeMs = now;
    lastClkState = clk;

    if (clk == LOW) {
      // Falling edge on CLK - direction is given by DT's current level.
      int dt = digitalRead(PIN_ENC_DT);
      return dt == HIGH ? ENC_NEXT : ENC_PREV;
      // NOTE: if turning the knob moves cards the wrong way once you have
      // the real hardware, just swap ENC_NEXT/ENC_PREV on this line.
    }
  }
  lastClkState = clk;
  return ENC_NONE;
}

static EncoderEvent pollButton() {
  bool isDown = digitalRead(PIN_ENC_SW) == LOW;
  unsigned long now = millis();

  if (isDown && !buttonWasDown) {
    buttonWasDown = true;
    buttonDownAtMs = now;
  } else if (!isDown && buttonWasDown) {
    buttonWasDown = false;
    unsigned long heldMs = now - buttonDownAtMs;
    return heldMs >= LONG_PRESS_MS ? ENC_LONG_PRESS : ENC_SHORT_PRESS;
  }
  return ENC_NONE;
}

EncoderEvent pollEncoder() {
  EncoderEvent rotation = pollRotation();
  if (rotation != ENC_NONE) return rotation;
  return pollButton();
}
