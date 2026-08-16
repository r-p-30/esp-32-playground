#include <WiFi.h>
#include "Config.h"
#include "Cards.h"
#include "DisplayUI.h"
#include "InputEncoder.h"
#include "BuzzerFX.h"
#include "TimeSync.h"
#include "RemoteControl.h"
#include "GameEngine.h"

enum DeviceMode { MODE_CARDS, MODE_GAME };

static int currentCard = 0;
static unsigned long lastCardChangeMs = 0;
static DeviceMode mode = MODE_CARDS;
static bool nightModeActive = false;
static unsigned long lastWifiCheckMs = 0;
#define WIFI_RECONNECT_CHECK_MS 10000UL

// Tracks whether the last frame we saw was already game-over, so
// playGameOver() fires exactly once on the losing hit rather than every
// loop iteration while the game-over screen sits there waiting for retry.
static bool gameOverSoundPlayed = false;

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
    showGameFrame();
  } else {
    showCard(currentCard);
  }
}

// Funnels every way into game mode (local long-press, remote toggle-on)
// through one place, same reasoning as changeToCard() - keeps the fresh
// Ready/Set/Go countdown and the game-over sound tracker consistent
// regardless of what triggered entry. forceHeartbeatNow() here (and in
// every other mode-change path below) is what keeps the site's stored
// toggle state in sync when the *button* is what changed it - without
// this, the site only finds out on its next periodic heartbeat (up to
// REMOTE_HEARTBEAT_INTERVAL_MS later), and the "Game mode"/"Night mode"
// checkboxes would silently disagree with the device until then. Harmless
// to also call it when the change came from the site itself - the site
// already knows in that case, so it's just a redundant, cheap heartbeat.
static void enterGameMode() {
  mode = MODE_GAME;
  initGame();
  gameOverSoundPlayed = false;
  playChime();
  showCurrentMode();
  forceHeartbeatNow();
}

// Shared by both ways out of game mode back to cards (local long/very-long
// press, and the site turning its Game mode toggle off) - see
// enterGameMode()'s comment for why forceHeartbeatNow() matters here too.
static void exitGameModeToCards() {
  mode = MODE_CARDS;
  playChime();
  showCurrentMode();
  forceHeartbeatNow();
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
    // exiting would redraw the mid-run game screen instead of cards).
    mode = MODE_CARDS;
    currentCard = 1;  // naturally lands on "good morning"
    lastCardChangeMs = millis();
    showCurrentMode();
    playChime();
  }
  forceHeartbeatNow();  // see enterGameMode()'s comment
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
    // Short press either jumps (mid-run) or restarts (on the game-over
    // screen) - a no-op during the Ready/Set/Go countdown, handled inside
    // gameJump() itself so a stray press can't pre-queue a jump. Long or
    // very long press both back out to card browsing - very long press
    // does NOT enter night mode from here, only card mode does that.
    if (ev == ENC_SHORT_PRESS) {
      if (isGameOver()) {
        gameRestart();
        gameOverSoundPlayed = false;
        showGameFrame();
      } else if (getGameSnapshot().state == GAME_RUNNING) {
        gameJump();
        playGameJump();
      }
    } else if (ev == ENC_LONG_PRESS || ev == ENC_VERY_LONG_PRESS) {
      exitGameModeToCards();
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
        enterGameMode();
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
    if (gameChange == 1) {
      enterGameMode();
    } else {
      exitGameModeToCards();
    }
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

  // A card edit from the site that *doesn't* also activate the card (the
  // "make this card active" checkbox left off) has no showCard to catch
  // above - if that edit targeted whatever's already on screen, redraw it
  // here so the change is visible immediately instead of only after
  // navigating away and back. Skipped when remoteJump already redrew this
  // same cycle (redundant), and while off the card browser entirely.
  int contentUpdateIdx = consumeRemoteContentUpdate();
  if (remoteJump < 0 && contentUpdateIdx == currentCard && mode == MODE_CARDS && !nightModeActive) {
    showCard(currentCard);
  }

  if (consumeRemoteBuzz()) {
    playIncomingMessage();
  }
  if (consumeRemoteAnimationTrigger() && mode == MODE_CARDS && !nightModeActive) {
    triggerAnimation();
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
  } else if (mode == MODE_GAME) {
    updateGameFrame();
    // Fires once on the transition into game-over (not every loop while
    // that screen sits there waiting for a retry press) - see
    // gameOverSoundPlayed's declaration above.
    if (isGameOver() && !gameOverSoundPlayed) {
      gameOverSoundPlayed = true;
      playGameOver();
    }
  }
}
