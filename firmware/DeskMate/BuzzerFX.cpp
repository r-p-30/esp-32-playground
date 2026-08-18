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

void playGameMiss() {
  // Non-blocking, same reasoning as playGameJump() - fires from inside the
  // game's per-frame stepping, not a rare one-off.
  tone(PIN_BUZZER, 600, 80);
}

void playGameLineClear(int lines) {
  // Reuses playGameJump()'s pitch family (high = good), but both frequency
  // and duration climb with `lines` so 1 vs 4 simultaneous clears are
  // clearly distinguishable by ear, not just by score change.
  if (lines < 1) return;
  if (lines > 4) lines = 4;
  int freq = 1400 + lines * 200;
  int dur = 60 + lines * 40;
  tone(PIN_BUZZER, freq, dur);
}

void playGameLevelUp() {
  tone(PIN_BUZZER, 2200, 150);
}

void playGameOverAlarm() {
  // Low and sustained, distinct from every other in-game beep (all of
  // which are short and higher-pitched) - reads as "the run is over," not
  // another gameplay blip. Non-blocking: tone()'s duration parameter lets
  // it keep ringing on its own for the whole flash window while the
  // caller's loop() keeps running (see updateGameFrame()/updateMoleFrame()
  // redrawing the blink during that same window).
  tone(PIN_BUZZER, 300, GAME_OVER_FLASH_DURATION_MS);
}
