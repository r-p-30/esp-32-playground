#include <Arduino.h>
#include "InputEncoder.h"
#include "Config.h"

// Quadrature state-table decode - validated in sanity-checks/encoder_test.
// A simple single-edge check (CLK falling + read DT) looks fine on the bench
// but produces false direction flips from mechanical bounce; this table
// method is what actually held up during testing.
static const int8_t QUAD_TABLE[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
static byte encoderHistory = 0;
static int8_t rotationAccum = 0;
static unsigned long lastRotationChangeMs = 0;

// If a partial transition (electrical noise while sitting idle, or a
// mechanical half-bounce) bumps rotationAccum without ever completing a
// full detent, it stays stuck at +-1 forever - meaning the next *real*
// turn has to fight past that leftover residue before it registers,
// which reads as "the knob stopped responding." Clearing it after a
// short idle window makes it self-heal instead of accumulating drift.
#define ROTATION_IDLE_RESET_MS 300

static bool buttonWasDown = false;
static unsigned long buttonDownAtMs = 0;

void initEncoder() {
  pinMode(PIN_ENC_CLK, INPUT_PULLUP);
  pinMode(PIN_ENC_DT, INPUT_PULLUP);
  pinMode(PIN_ENC_SW, INPUT_PULLUP);  // KY-040 button reads LOW when pressed
  encoderHistory = (digitalRead(PIN_ENC_CLK) << 1) | digitalRead(PIN_ENC_DT);
}

static EncoderEvent pollRotation() {
  unsigned long now = millis();
  if (rotationAccum != 0 && (now - lastRotationChangeMs) > ROTATION_IDLE_RESET_MS) {
    rotationAccum = 0;
  }

  encoderHistory <<= 2;
  encoderHistory |= (digitalRead(PIN_ENC_CLK) << 1) | digitalRead(PIN_ENC_DT);
  int8_t change = QUAD_TABLE[encoderHistory & 0x0F];

  if (change != 0) {
    rotationAccum += change;
    lastRotationChangeMs = now;
    // Each detent produces 2 valid transitions - only fire once per detent.
    if (rotationAccum >= 2) {
      rotationAccum = 0;
      return ENC_NEXT;
      // NOTE: if turning the knob moves cards the wrong way once you have
      // the real hardware, swap ENC_NEXT/ENC_PREV in these two branches.
    }
    if (rotationAccum <= -2) {
      rotationAccum = 0;
      return ENC_PREV;
    }
  }
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
    if (heldMs >= VERY_LONG_PRESS_MS) return ENC_VERY_LONG_PRESS;
    if (heldMs >= LONG_PRESS_MS) return ENC_LONG_PRESS;
    return ENC_SHORT_PRESS;
  }
  return ENC_NONE;
}

EncoderEvent pollEncoder() {
  EncoderEvent rotation = pollRotation();
  if (rotation != ENC_NONE) return rotation;
  return pollButton();
}
