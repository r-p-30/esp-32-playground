#include <WiFi.h>
#include <esp_system.h>
#include "Config.h"
#include "Cards.h"
#include "DisplayUI.h"
#include "InputEncoder.h"
#include "BuzzerFX.h"
#include "TimeSync.h"
#include "RemoteControl.h"
#include "Game.h"

enum DeviceMode { MODE_CARDS, MODE_GAME_MENU, MODE_GAME };

static int currentCard = 0;
static unsigned long lastCardChangeMs = 0;
static DeviceMode mode = MODE_CARDS;
// Which GAMES[] entry the menu cursor is on / which one is currently
// running. Deliberately not reset when leaving the menu or a game - so
// coming back lands back where you left off, same as currentCard.
static int selectedGame = GAME_DINO_JUMP;
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
    showGameFrame();
  } else if (mode == MODE_GAME_MENU) {
    showGameMenu(selectedGame);
  } else {
    showCard(currentCard);
  }
}

// Funnels every way into the game picker menu (local long-press, remote
// toggle-on) through one place, same reasoning as changeToCard() - keeps
// things consistent regardless of what triggered entry. forceHeartbeatNow()
// here (and in every other mode-change path below) is what keeps the site's
// stored toggle state in sync when the *button* is what changed it -
// without this, the site only finds out on its next periodic heartbeat (up
// to REMOTE_HEARTBEAT_INTERVAL_MS later), and the "Game mode"/"Night mode"
// checkboxes would silently disagree with the device until then. Harmless
// to also call it when the change came from the site itself - the site
// already knows in that case, so it's just a redundant, cheap heartbeat.
static void enterGameMenu() {
  mode = MODE_GAME_MENU;
  playChime();
  showGameMenu(selectedGame);
  forceHeartbeatNow();
}

// Short press on the menu picks whichever GAMES[] entry is currently
// selected - a no-op for a stub (`implemented: false`), so pressing on
// "Whack-a-mole (soon)" just does nothing rather than launching a blank
// screen. See enterGameMenu()'s comment for forceHeartbeatNow().
static void enterSelectedGame() {
  if (!GAMES[selectedGame].implemented) return;
  mode = MODE_GAME;
  GAMES[selectedGame].enter();
  playChime();
  forceHeartbeatNow();
}

// Long/very-long press from inside a game now backs out to the picker menu
// rather than straight to cards (see enterSelectedGame()/exitGameModeToCards()
// for the rest of the in/out path) - exitGameModeToCards() is what a second
// long/very-long press from the menu itself falls through to.
static void exitGameToMenu() {
  mode = MODE_GAME_MENU;
  playChime();
  showGameMenu(selectedGame);
  forceHeartbeatNow();
}

// Shared by every way out back to cards - local long/very-long press from
// the game menu, and the site turning its Game mode toggle off (which can
// land while either a game or the menu is showing) - see enterGameMenu()'s
// comment for why forceHeartbeatNow() matters here too.
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
  forceHeartbeatNow();  // see enterGameMenu()'s comment
}

// Human label for esp_reset_reason() - printed first thing in setup(),
// before anything that could itself fail, so if the board is silently
// rebooting (watchdog, brownout, panic) instead of just losing WiFi, the
// very next boot's log says why instead of us guessing. RTC-backed, so it
// survives the reset that caused it.
static const char* resetReasonLabel(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON:   return "power-on";
    case ESP_RST_EXT:       return "external pin";
    case ESP_RST_SW:        return "software (esp_restart)";
    case ESP_RST_PANIC:     return "PANIC (crash)";
    case ESP_RST_INT_WDT:   return "interrupt watchdog";
    case ESP_RST_TASK_WDT:  return "task watchdog (loop() stalled too long)";
    case ESP_RST_WDT:       return "other watchdog";
    case ESP_RST_BROWNOUT:  return "brownout (power sag)";
    case ESP_RST_SDIO:      return "SDIO";
    default:                return "unknown/other";
  }
}

void setup() {
  Serial.begin(115200);
  Serial.print("Reset reason: ");
  Serial.println(resetReasonLabel(esp_reset_reason()));
  Serial.printf("Free heap at boot: %u bytes\n", ESP.getFreeHeap());

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
    // Short press is entirely whatever the active game's own
    // onShortPress() does (jump/restart for Dino, including any sound
    // that goes with it) - this file doesn't need to know what that means
    // for a given game. Rotation is likewise entirely the active game's
    // own onRotate() (Dino ignores it; Whack-a-mole moves its cursor).
    // Long or very long press both back out to the game menu (not
    // straight to cards anymore) - very long press does NOT enter night
    // mode from here, only card mode does that.
    if (ev == ENC_SHORT_PRESS) {
      GAMES[selectedGame].onShortPress();
    } else if (ev == ENC_NEXT) {
      GAMES[selectedGame].onRotate(1);
    } else if (ev == ENC_PREV) {
      GAMES[selectedGame].onRotate(-1);
    } else if (ev == ENC_LONG_PRESS || ev == ENC_VERY_LONG_PRESS) {
      exitGameToMenu();
    }
  } else if (mode == MODE_GAME_MENU) {
    // Rotation moves the arrow up/down the list (with a nav blip, same as
    // browsing cards) and wraps at the ends, same modulo pattern as
    // nextVisibleIndex(). Short press launches whichever game is
    // highlighted; long/very-long press backs all the way out to cards.
    switch (ev) {
      case ENC_NEXT:
        selectedGame = (selectedGame + 1) % GAME_COUNT;
        beepCardChange();
        showGameMenu(selectedGame);
        break;

      case ENC_PREV:
        selectedGame = (selectedGame - 1 + GAME_COUNT) % GAME_COUNT;
        beepCardChange();
        showGameMenu(selectedGame);
        break;

      case ENC_SHORT_PRESS:
        enterSelectedGame();
        break;

      case ENC_LONG_PRESS:
      case ENC_VERY_LONG_PRESS:
        exitGameModeToCards();
        break;

      case ENC_NONE:
      default:
        break;
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
        enterGameMenu();
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

  // gameMode reported to the site covers both the picker menu and an
  // actual game in progress - from the site's side "Game mode" just means
  // "off the card browser", same as what the local long-press now opens
  // into (the menu) rather than jumping straight into Dino. selectedGame
  // is reported regardless of mode (harmless while in cards - the site
  // only reads it when gameMode is also true), same as currentCard always
  // being reported even in night mode.
  loopRemoteControl(currentCard, nightModeActive, mode != MODE_CARDS, selectedGame);

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
      enterGameMenu();
    } else {
      exitGameModeToCards();
    }
  }

  // A specific game picked from the site's game list - jumps straight into
  // it (skipping the picker menu), and fires on every new selection
  // regardless of whether game mode was already on, so this also covers
  // "swap the active game while one's already running." Same implemented-
  // only guard as enterSelectedGame() (a stub selection from the site is a
  // no-op, not a launch into a blank screen).
  int remoteGameSelect = consumeRemoteGameSelect();
  if (remoteGameSelect >= 0 && remoteGameSelect < GAME_COUNT &&
      GAMES[remoteGameSelect].implemented && !nightModeActive) {
    mode = MODE_GAME;
    selectedGame = remoteGameSelect;
    GAMES[selectedGame].enter();
    playChime();
    forceHeartbeatNow();
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
    GAMES[selectedGame].step();
  }
  // mode == MODE_GAME_MENU: no continuous per-loop redraw - the menu only
  // repaints on ENC_NEXT/ENC_PREV/ENC_SHORT_PRESS (see the switch above),
  // there's no animation running while it just sits there.
}
