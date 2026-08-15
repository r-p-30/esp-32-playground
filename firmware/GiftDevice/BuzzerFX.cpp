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
