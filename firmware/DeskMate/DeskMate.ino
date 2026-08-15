#include <WiFi.h>
#include "Config.h"
#include "Cards.h"
#include "DisplayUI.h"
#include "InputEncoder.h"
#include "BuzzerFX.h"
#include "TimeSync.h"
#include "RemoteControl.h"

enum DeviceMode { MODE_CARDS, MODE_GAME };

static int currentCard = 0;
static unsigned long lastCardChangeMs = 0;
static DeviceMode mode = MODE_CARDS;
static bool nightModeActive = false;
static unsigned long lastWifiCheckMs = 0;
#define WIFI_RECONNECT_CHECK_MS 10000UL

// Funnels every card change (manual, remote, or carousel) through one
// place so the beep and the carousel timer stay consistent regardless of
// what triggered the change.
static void changeToCard(int index) {
  currentCard = index;
  beepCardChange();
  showCard(currentCard);
  lastCardChangeMs = millis();
}

static void showCurrentMode() {
  if (mode == MODE_GAME) {
    showGamePlaceholder();
  } else {
    showCard(currentCard);
  }
}

static void setNightMode(bool active) {
  if (active == nightModeActive) return;
  nightModeActive = active;
  if (nightModeActive) {
    playChime();
    enterNightMode();
  } else {
    exitNightMode();
    // Fresh start after night mode - always back to card browsing, even if
    // game mode was active before night mode was entered (mode isn't
    // touched by entering night mode, so without this reset here,
    // exiting would redraw the game placeholder instead of cards).
    mode = MODE_CARDS;
    currentCard = 1;  // naturally lands on "good morning"
    lastCardChangeMs = millis();
    showCurrentMode();
    playChime();
  }
}

void setup() {
  Serial.begin(115200);

  initBuzzer();
  initEncoder();
  initDisplay();

  syncTime();  // bounded attempt; leaves WiFi connected either way if it succeeds

  // Card 0 is the clock - only meaningful once syncTime() actually landed a
  // real timestamp this boot. Offline (or sync failed), land on card 1
  // ("good morning") instead so boot never shows a stuck "--:--:--".
  currentCard = isTimeSynced() ? 0 : 1;
  showCard(currentCard);
  lastCardChangeMs = millis();

  if (WiFi.status() == WL_CONNECTED) {
    beginRemoteControl();
  }
}

void loop() {
  EncoderEvent ev = pollEncoder();

  if (nightModeActive) {
    // Rotation is suppressed while night mode is active - it's a dedicated
    // clock, not another browsable screen. Any press (short, long, or very
    // long) exits back to cards - no reason to make the user guess
    // the right press length just to get out.
    if (ev == ENC_SHORT_PRESS || ev == ENC_LONG_PRESS || ev == ENC_VERY_LONG_PRESS) {
      setNightMode(false);
    }
  } else if (mode == MODE_GAME) {
    // Short press is reserved for gameplay input once it's built. Long or
    // very long press both back out to card browsing - very long press
    // does NOT enter night mode from here, only card mode does that.
    if (ev == ENC_LONG_PRESS || ev == ENC_VERY_LONG_PRESS) {
      mode = MODE_CARDS;
      playChime();
      showCurrentMode();
    }
  } else {
    switch (ev) {
      case ENC_NEXT:
        changeToCard(nextVisibleIndex(currentCard, 1));
        break;

      case ENC_PREV:
        changeToCard(nextVisibleIndex(currentCard, -1));
        break;

      case ENC_SHORT_PRESS:
        triggerAnimation();
        break;

      case ENC_LONG_PRESS:
        mode = MODE_GAME;
        playChime();
        showCurrentMode();
        break;

      case ENC_VERY_LONG_PRESS:
        setNightMode(true);
        break;

      case ENC_NONE:
      default:
        break;
    }
  }

  // WiFi is meant to stay connected permanently now (USB-powered), but a
  // router restart or signal drop can still knock it out mid-runtime -
  // check occasionally and reconnect rather than staying offline until
  // the next reboot. Also covers the boot-time case where WiFi wasn't up
  // yet when setup() tried to start remote control.
  unsigned long now = millis();
  if (now - lastWifiCheckMs >= WIFI_RECONNECT_CHECK_MS) {
    lastWifiCheckMs = now;
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.reconnect();
    } else {
      beginRemoteControl();  // no-op once already started
    }
  }

  loopRemoteControl(currentCard, nightModeActive, mode == MODE_GAME);

  // Edge-triggered on the site's stored value actually changing, so a
  // local button toggle isn't immediately re-fought by an unchanged
  // remote default on the next poll - see RemoteControl.h.
  int nightChange = consumeRemoteNightModeChange();
  if (nightChange >= 0) {
    setNightMode(nightChange == 1);
  }
  int gameChange = consumeRemoteGameModeChange();
  if (gameChange >= 0 && !nightModeActive) {
    mode = (gameChange == 1) ? MODE_GAME : MODE_CARDS;
    playChime();
    showCurrentMode();
  }

  int remoteJump = consumeRemoteCardJump();
  if (remoteJump >= 0) {
    mode = MODE_CARDS;
    if (nightModeActive) {
      nightModeActive = false;
      exitNightMode();
    }
    changeToCard(remoteJump % NUM_CARDS);
  }
  if (consumeRemoteBuzz()) {
    playIncomingMessage();
  }
  if (consumeRemoteAnimationTrigger() && mode == MODE_CARDS && !nightModeActive) {
    triggerAnimation();
  }
  if (consumeRandomNotify()) {
    playRandomPing();
  }
  if (consumeIdentifyPing()) {
    playIdentifyPing();
    flashIdentify();
  }

  if (nightModeActive) {
    renderNightMode();
  } else if (mode == MODE_CARDS) {
    if (isCarouselEnabled() && (millis() - lastCardChangeMs) >= getCarouselIntervalMs()) {
      changeToCard(nextVisibleIndex(currentCard, 1));
    }
    updateDisplayAnimation();
  }
}
