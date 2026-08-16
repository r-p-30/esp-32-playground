#include <Arduino.h>
#include "BuzzerFX.h"
#include "Config.h"

void initBuzzer() {
  pinMode(PIN_BUZZER, OUTPUT);
  noTone(PIN_BUZZER);
}

void beepCardChange() {
  tone(PIN_BUZZER, 1000, 40);
}

void playChime() {
  // Simple two-note chime. Blocking delay here is fine - it only runs
  // on a deliberate long-press, not every loop iteration.
  tone(PIN_BUZZER, 880, 120);
  delay(130);
  tone(PIN_BUZZER, 1320, 160);
  delay(170);
  noTone(PIN_BUZZER);
}

void playIncomingMessage() {
  // Three quick beeps - only fires on a genuinely new remote update
  // (rate-limited by RemoteControl's revision check), so a short blocking
  // delay here is fine.
  for (int i = 0; i < 3; i++) {
    tone(PIN_BUZZER, 1500, 80);
    delay(110);
  }
  noTone(PIN_BUZZER);
}

void playIdentifyPing() {
  tone(PIN_BUZZER, 1200, 60);
  delay(90);
  tone(PIN_BUZZER, 1200, 60);
  delay(90);
  noTone(PIN_BUZZER);
}

void playGameJump() {
  // tone() with a duration is non-blocking (returns immediately) - safe to
  // call from inside the game's per-frame input handling without stalling
  // the loop.
  tone(PIN_BUZZER, 1800, 60);
}

void playGameOver() {
  // Three descending notes - only fires once per collision, so a short
  // blocking delay here is fine (same reasoning as playIncomingMessage()).
  tone(PIN_BUZZER, 700, 120);
  delay(130);
  tone(PIN_BUZZER, 500, 120);
  delay(130);
  tone(PIN_BUZZER, 300, 200);
  delay(210);
  noTone(PIN_BUZZER);
}
