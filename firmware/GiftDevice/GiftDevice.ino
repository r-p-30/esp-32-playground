#include "Config.h"
#include "Cards.h"
#include "DisplayUI.h"
#include "InputEncoder.h"
#include "BuzzerFX.h"
#include "TimeSync.h"

static int currentCard = 0;

void setup() {
  Serial.begin(115200);

  initBuzzer();
  initEncoder();
  initDisplay();

  syncTime();  // brief, bounded attempt - falls back gracefully if it fails

  showCard(currentCard);
}

void loop() {
  switch (pollEncoder()) {
    case ENC_NEXT:
      currentCard = (currentCard + 1) % NUM_CARDS;
      beepCardChange();
      showCard(currentCard);
      break;

    case ENC_PREV:
      currentCard = (currentCard - 1 + NUM_CARDS) % NUM_CARDS;
      beepCardChange();
      showCard(currentCard);
      break;

    case ENC_SHORT_PRESS:
      triggerAnimation();
      break;

    case ENC_LONG_PRESS:
      playChime();
      break;

    case ENC_NONE:
    default:
      break;
  }

  updateDisplayAnimation();
  checkForSync();  // no-op in Phase 1, real logic added in Phase 2
}
