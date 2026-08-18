#include <Arduino.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "RemoteControl.h"
#include "Cards.h"
#include "Config.h"
#include "RemoteApi.h"

// ---- Networking task (core 0) ----
// WebSocketsClient::loop() can block for a long time on a bad connection -
// in particular the TCP/TLS connect it does on every reconnect attempt has
// no short bound, so a flaky link to the site could stall this call for
// many seconds. That used to run inline in DeskMate.ino's loop(), meaning
// a bad remote connection could freeze the encoder/game/buzzer for however
// long that one call took (this is what turned out to be causing
// "random" game freezes - see the reconnect-churn pattern in
// docs/remote-api-spec.md's history/commit log around this change).
//
// Fix: the WebSocketsClient itself, and everything that can block on it,
// now lives entirely on its own FreeRTOS task pinned to the second core -
// see beginRemoteControl(). It can block all it wants without touching
// anything input-driven. Only two things cross between the two tasks, both
// guarded by wsMutex: the latest raw text frame received from the site
// (incomingMessage) and the next one this task should send
// (outgoingMessage), plus a justConnected flag. Every other piece of
// state below (applyStateJson()'s parsing/Cards.cpp mutation, the
// pending-* flags, sendCardsReport()'s JSON building) stays exclusively on
// the main-loop task, completely unchanged from before this split - only
// the raw socket I/O moved.
static WebSocketsClient webSocket;
static bool wsStarted = false;
static SemaphoreHandle_t wsMutex = nullptr;

static String incomingMessage;
static bool hasIncomingMessage = false;
static String outgoingMessage;
static bool hasOutgoingMessage = false;
static bool justConnected = false;

// Bounded wait on the main-loop side - the critical sections below are all
// just copying a String/bool, so this should never actually get close to
// timing out; it's a safety ceiling, not a real budget, so the main loop
// can never be blocked on this mutex for long even in a pathological case.
#define WS_MUTEX_WAIT_MS  20

static unsigned long lastHeartbeatSentMs = 0;

static long lastAppliedRevision = -1;

static int pendingCardJump = -1;
static int pendingGameSelect = -1;
static int pendingContentUpdateIndex = -1;
static bool pendingBuzz = false;
static bool pendingAnimationTrigger = false;
static bool pendingIdentify = false;

// Continuous state (not revision-gated - always reflects the latest push).
static bool carouselEnabled = false;
static unsigned long carouselIntervalMs = 5000;
static unsigned long carouselStartedAtMs = 0;
static bool carouselWasEnabledLastPoll = false;

// Hard local safety cap - the carousel keeps the display + logic active
// continuously. Regardless of what the site says, it auto-stops after
// this long; the site has to explicitly set carouselEnabled false then
// true again (a real stop/restart) to get another window, rather than it
// running forever unattended.
#define CAROUSEL_MAX_RUNTIME_MS (60UL * 60UL * 1000UL)

static bool nightModeRawLastPoll = false;
static bool gameModeRawLastPoll = false;
static int pendingNightModeChange = -1;  // -1 none, 0 off, 1 on
static int pendingGameModeChange = -1;
// The very first state push after connecting just seeds the two
// *RawLastPoll baselines above - it must NOT be treated as a change to act
// on, or a device reboot/reflash while the site still has an old session's
// nightModeEnabled/gameModeEnabled: true persisted would immediately
// re-enter that mode on its own instead of always booting to cards. Also
// used below to guard the one-shot `activeGame` field for the exact same
// reason - see applyStateJson()'s isFirstStatePoll local.
static bool firstStatePollSeen = false;

// Expects "wss://host/path" form.
static bool splitWsUrl(const char* urlStr, String& outHost, String& outPath) {
  String url = urlStr;
  if (!url.startsWith("wss://")) return false;

  int hostStart = 6;
  int pathStart = url.indexOf('/', hostStart);
  outHost = (pathStart == -1) ? url.substring(hostStart) : url.substring(hostStart, pathStart);
  outPath = (pathStart == -1) ? "/" : url.substring(pathStart);
  return outHost.length() > 0;
}

// RemoteApi.example.h's placeholder value - if it hasn't been changed,
// there's no real endpoint configured yet, so skip entirely rather than
// endlessly retrying a connection to a nonexistent host.
static bool remoteApiConfigured() {
  return strcmp(REMOTE_API_URL, "wss://your-site.example/ws/device") != 0;
}

// Hands msg off to the network task to actually send - only that task
// touches `webSocket` directly (see remoteControlTask()), so every send
// from the main-loop side goes through here instead of calling
// webSocket.sendTXT() itself.
static void queueOutgoing(const String& msg) {
  if (xSemaphoreTake(wsMutex, pdMS_TO_TICKS(WS_MUTEX_WAIT_MS)) == pdTRUE) {
    outgoingMessage = msg;
    hasOutgoingMessage = true;
    xSemaphoreGive(wsMutex);
  }
}

static TextAlignH parseAlignH(const char* s) {
  if (s == nullptr) return ALIGN_H_CENTER;
  if (strcmp(s, "left") == 0) return ALIGN_H_LEFT;
  if (strcmp(s, "right") == 0) return ALIGN_H_RIGHT;
  return ALIGN_H_CENTER;
}

static TextAlignV parseAlignV(const char* s) {
  if (s == nullptr) return ALIGN_V_MIDDLE;
  if (strcmp(s, "top") == 0) return ALIGN_V_TOP;
  if (strcmp(s, "bottom") == 0) return ALIGN_V_BOTTOM;
  return ALIGN_V_MIDDLE;
}

// Unchanged from before the task split - still only ever called from the
// main-loop task (loopRemoteControl() below), just now fed by the mailbox
// instead of directly by onWsEvent().
static void applyStateJson(const uint8_t* payload, size_t length) {
  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, payload, length) != DeserializationError::Ok) return;

  // Captured before firstStatePollSeen flips true a few lines down - the
  // activeGame guard further below needs to know whether *this* call is the
  // device's first poll since connecting, but by the time execution reaches
  // it firstStatePollSeen has already been set true by the night/game-mode
  // edge-detection above it.
  bool isFirstStatePoll = !firstStatePollSeen;

  // Continuous config - applied from every push, independent of whether
  // revision changed (ongoing settings, not events).
  bool rawCarouselEnabled = doc["carouselEnabled"] | false;
  if (rawCarouselEnabled && !carouselWasEnabledLastPoll) {
    // False -> true transition: a genuine (re)start, begin a fresh window.
    carouselStartedAtMs = millis();
  }
  carouselWasEnabledLastPoll = rawCarouselEnabled;
  carouselEnabled = rawCarouselEnabled;

  long carouselSec = doc["carouselIntervalSec"] | 5L;
  if (carouselSec < 1) carouselSec = 1;
  carouselIntervalMs = (unsigned long)carouselSec * 1000UL;

  // Night/game mode: edge-triggered on the site's value changing, not
  // blindly reasserted - see RemoteControl.h for why. Skipped on the very
  // first poll (see firstStatePollSeen's comment) - that one only seeds
  // the baseline, so the device always boots to cards regardless of
  // whatever the site had persisted from before.
  bool rawNight = doc["nightModeEnabled"] | false;
  if (firstStatePollSeen && rawNight != nightModeRawLastPoll) {
    pendingNightModeChange = rawNight ? 1 : 0;
  }
  nightModeRawLastPoll = rawNight;

  bool rawGame = doc["gameModeEnabled"] | false;
  if (firstStatePollSeen && rawGame != gameModeRawLastPoll) {
    pendingGameModeChange = rawGame ? 1 : 0;
  }
  gameModeRawLastPoll = rawGame;
  firstStatePollSeen = true;

  // One-shot actions below - only fire on a genuinely new revision.
  long revision = doc["revision"] | -1L;
  if (revision >= 0 && revision != lastAppliedRevision) {
    lastAppliedRevision = revision;

    if (!doc["cardTextIndex"].isNull()) {
      int idx = doc["cardTextIndex"].as<int>();
      // Flagged regardless of whether this same push also includes
      // showCard - if it does, changeToCard() in DeskMate.ino already
      // redraws and this is just a harmless redundant one; if it
      // doesn't (the common "editing the card already on screen" case),
      // this is the only thing that tells the main loop to redraw.
      pendingContentUpdateIndex = idx;

      if (!doc["cardText"].isNull()) {
        setCardText(idx, doc["cardText"].as<const char*>());
      }
      // Layout - only visible on the ANIM_BOUNCE (custom/reserved-slot)
      // card, see Card's comment in Cards.h. Both sent together whenever
      // either changes, so a partial update can't leave one axis stale.
      if (!doc["cardTextAlignH"].isNull() || !doc["cardTextAlignV"].isNull()) {
        setCardAlignment(idx,
                          parseAlignH(doc["cardTextAlignH"] | (const char*)nullptr),
                          parseAlignV(doc["cardTextAlignV"] | (const char*)nullptr));
      }
      if (doc["cardCornerEmoji"].is<JsonArray>()) {
        JsonArray corners = doc["cardCornerEmoji"].as<JsonArray>();
        for (int c = 0; c < 4 && c < (int)corners.size(); c++) {
          setCardCornerEmoji(idx, c, emojiShortcodeToGlyph(corners[c].as<const char*>()));
        }
      }
      if (!doc["cardAnimatedEmojiDisableMask"].isNull()) {
        setCardAnimatedEmojiDisableMask(idx, doc["cardAnimatedEmojiDisableMask"].as<uint8_t>());
      }
    }
    if (!doc["showCard"].isNull()) {
      pendingCardJump = doc["showCard"].as<int>();
    }
    if (!doc["activeGame"].isNull() && !isFirstStatePoll) {
      // Same one-time-nudge semantics as showCard above, but jumps
      // straight into a specific game (Game.h's GameId index) instead of
      // just opening the on-device picker menu - lets the site both start
      // a game from cards mode and swap the active game while one's
      // already running, without the physical knob's menu-first flow.
      //
      // Guarded on !isFirstStatePoll for the same reason
      // nightModeEnabled/gameModeEnabled are (firstStatePollSeen's
      // comment): the site only clears `activeGame` back to neutral on the
      // *next* action after a game was picked (state.py's apply_update()),
      // not on its own, so it sits there in the persisted state
      // indefinitely if the device reboots/reflashes right after someone
      // picked a game from the site. Without this guard that stale value
      // would replay here on reconnect and launch straight into that game
      // instead of always starting in cards mode - this was the actual
      // bug: booting to cards worked fine before, this field just wasn't
      // covered by the existing guard.
      pendingGameSelect = doc["activeGame"].as<int>();
    }
    if (doc["buzz"] | false) {
      pendingBuzz = true;
    }
    if (doc["triggerAnimation"] | false) {
      pendingAnimationTrigger = true;
    }
    if (doc["identifyPing"] | false) {
      pendingIdentify = true;
    }
    if (!doc["cardAnimationDurationIndex"].isNull() && !doc["cardAnimationDurationMs"].isNull()) {
      setCardAnimationDuration(doc["cardAnimationDurationIndex"].as<int>(),
                                doc["cardAnimationDurationMs"].as<uint16_t>());
    }
  }
}

static const char* alignHToString(TextAlignH h) {
  switch (h) {
    case ALIGN_H_LEFT: return "left";
    case ALIGN_H_RIGHT: return "right";
    default: return "center";
  }
}

static const char* alignVToString(TextAlignV v) {
  switch (v) {
    case ALIGN_V_TOP: return "top";
    case ALIGN_V_BOTTOM: return "bottom";
    default: return "middle";
  }
}

// Sent once right after connecting. The device's RAM is a more reliable
// source of truth for "what's actually in each card slot" than the
// site's own persisted copy - card text only ever changes here via a
// server push, so it always reflects the last one that landed, and it
// survives things that can wipe the site's disk (a redeploy, an idle
// restart on free hosting). The server treats this as authoritative and
// overwrites its local cache with it - see /ws/device in app.py. Still
// only ever called from the main-loop task (reads Cards.cpp, same as
// before) - just hands its output to queueOutgoing() instead of sending
// it directly.
static void sendCardsReport() {
  JsonDocument doc;
  JsonArray arr = doc["cardsReport"].to<JsonArray>();
  for (int i = 0; i < NUM_CARDS; i++) {
    const Card& c = getCardContent(i);
    JsonObject o = arr.add<JsonObject>();
    o["index"] = i;
    o["text"] = c.text;
    o["animationDurationMs"] = c.animationDurationMs;
    o["populated"] = c.populated;
    o["alignH"] = alignHToString(c.alignH);
    o["alignV"] = alignVToString(c.alignV);
    JsonArray corners = o["cornerEmoji"].to<JsonArray>();
    for (int k = 0; k < 4; k++) {
      corners.add(glyphToEmojiShortcode(c.cornerEmoji[k]));
    }
    o["animatedEmojiDisableMask"] = c.animatedEmojiDisableMask;
  }
  String out;
  serializeJson(doc, out);
  queueOutgoing(out);
}

// Runs on the network task (core 0) - only touches the wsMutex-guarded
// mailbox, never Cards.cpp or the pending-* flags directly, so it can't
// race the main-loop task.
static void onWsEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_TEXT:
      if (xSemaphoreTake(wsMutex, pdMS_TO_TICKS(WS_MUTEX_WAIT_MS)) == pdTRUE) {
        incomingMessage = String(payload, length);
        hasIncomingMessage = true;
        xSemaphoreGive(wsMutex);
      }
      break;
    case WStype_CONNECTED:
      Serial.printf("Remote control connected. Free heap: %u bytes\n", ESP.getFreeHeap());
      if (xSemaphoreTake(wsMutex, pdMS_TO_TICKS(WS_MUTEX_WAIT_MS)) == pdTRUE) {
        justConnected = true;
        xSemaphoreGive(wsMutex);
      }
      break;
    case WStype_DISCONNECTED:
      Serial.printf("Remote control disconnected - will auto-retry. Free heap: %u bytes\n", ESP.getFreeHeap());
      break;
    default:
      break;
  }
}

// The network task's body - owns `webSocket` exclusively from here on.
// Loops forever: pumps the socket (this is the call that can block for a
// while on a bad connection - now harmless, since nothing else depends on
// this task running promptly), then ships out anything the main-loop task
// queued via queueOutgoing(). vTaskDelay(1) is just a yield so this task
// doesn't starve lower-priority tasks/the idle-task watchdog on its core
// during a tight reconnect loop.
static void remoteControlTask(void* pvParameters) {
  (void)pvParameters;

  static String extraHeaders = String("X-Device-Key: ") + REMOTE_API_KEY;
  webSocket.setExtraHeaders(extraHeaders.c_str());
  webSocket.onEvent(onWsEvent);
  webSocket.setReconnectInterval(5000);
  // Protocol-level ping/pong so a half-dead connection (e.g. a NAT
  // timeout that never sends a clean close) gets noticed and reconnected
  // instead of silently going stale for a permanent connection.
  webSocket.enableHeartbeat(15000, 3000, 2);

  String host, path;
  splitWsUrl(REMOTE_API_URL, host, path);  // already validated in beginRemoteControl() before this task was spawned
  webSocket.beginSSL(host.c_str(), 443, path.c_str());

  for (;;) {
    webSocket.loop();

    String toSend;
    bool shouldSend = false;
    if (xSemaphoreTake(wsMutex, pdMS_TO_TICKS(WS_MUTEX_WAIT_MS)) == pdTRUE) {
      if (hasOutgoingMessage) {
        toSend = outgoingMessage;
        hasOutgoingMessage = false;
        shouldSend = true;
      }
      xSemaphoreGive(wsMutex);
    }
    if (shouldSend) {
      webSocket.sendTXT(toSend);
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void beginRemoteControl() {
  if (wsStarted || !remoteApiConfigured()) return;

  String host, path;
  if (!splitWsUrl(REMOTE_API_URL, host, path)) return;

  wsMutex = xSemaphoreCreateMutex();
  if (wsMutex == nullptr) return;  // out of memory - stay offline rather than risk an unguarded mailbox

  wsStarted = true;
  // Core 0 - Arduino's own setup()/loop() task runs on core 1
  // (CONFIG_ARDUINO_RUNNING_CORE), so this fully separates network I/O
  // from the input/game loop, not just logically but physically. 8KB
  // stack - WebSocketsClient + the TLS handshake it does under beginSSL()
  // is fairly stack-hungry; the default loop task's stack wouldn't be a
  // fair comparison since this task does nothing but this.
  xTaskCreatePinnedToCore(remoteControlTask, "RemoteWS", 8192, nullptr, 1, nullptr, 0);
}

static void sendHeartbeatIfDue(int currentCard, bool nightModeActive, bool inGameMode, int activeGame) {
  if (!wsStarted) return;

  unsigned long now = millis();
  if (lastHeartbeatSentMs != 0 && (now - lastHeartbeatSentMs) < REMOTE_HEARTBEAT_INTERVAL_MS) {
    return;
  }
  lastHeartbeatSentMs = now;

  char body[144];
  snprintf(body, sizeof(body),
           "{\"currentCard\":%d,\"nightMode\":%s,\"gameMode\":%s,\"activeGame\":%d,\"uptimeSec\":%lu}",
           currentCard,
           nightModeActive ? "true" : "false",
           inGameMode ? "true" : "false",
           activeGame,
           millis() / 1000UL);
  queueOutgoing(body);
}

void loopRemoteControl(int currentCard, bool nightModeActive, bool inGameMode, int activeGame) {
  if (!wsStarted) return;

  // Drain anything the network task queued up since our last visit -
  // applyStateJson()/sendCardsReport() both stay on this (main-loop) task,
  // completely unchanged from before the task split, they're just fed by
  // the mailbox instead of being called directly from onWsEvent().
  String incoming;
  bool hadIncoming = false;
  bool connectedJustNow = false;
  if (xSemaphoreTake(wsMutex, pdMS_TO_TICKS(WS_MUTEX_WAIT_MS)) == pdTRUE) {
    if (hasIncomingMessage) {
      incoming = incomingMessage;
      hasIncomingMessage = false;
      hadIncoming = true;
    }
    if (justConnected) {
      justConnected = false;
      connectedJustNow = true;
    }
    xSemaphoreGive(wsMutex);
  }

  if (connectedJustNow) {
    sendCardsReport();
  }
  if (hadIncoming) {
    applyStateJson((const uint8_t*)incoming.c_str(), incoming.length());
  }

  sendHeartbeatIfDue(currentCard, nightModeActive, inGameMode, activeGame);
}

void forceHeartbeatNow() {
  // 0 is sendHeartbeatIfDue()'s own "never sent yet" sentinel, which skips
  // the interval check entirely - reusing it here is what makes the next
  // loopRemoteControl() call send right away instead of waiting out
  // REMOTE_HEARTBEAT_INTERVAL_MS. Only ever touched by the main-loop task
  // (same as before the task split), so no locking needed here.
  lastHeartbeatSentMs = 0;
}

int consumeRemoteCardJump() {
  int jump = pendingCardJump;
  pendingCardJump = -1;
  return jump;
}

int consumeRemoteGameSelect() {
  int idx = pendingGameSelect;
  pendingGameSelect = -1;
  return idx;
}

int consumeRemoteContentUpdate() {
  int idx = pendingContentUpdateIndex;
  pendingContentUpdateIndex = -1;
  return idx;
}

bool consumeRemoteBuzz() {
  bool buzz = pendingBuzz;
  pendingBuzz = false;
  return buzz;
}

bool consumeRemoteAnimationTrigger() {
  bool trigger = pendingAnimationTrigger;
  pendingAnimationTrigger = false;
  return trigger;
}

bool consumeIdentifyPing() {
  bool id = pendingIdentify;
  pendingIdentify = false;
  return id;
}

bool isCarouselEnabled() {
  if (!carouselEnabled) return false;
  if (millis() - carouselStartedAtMs > CAROUSEL_MAX_RUNTIME_MS) return false;
  return true;
}

unsigned long getCarouselIntervalMs() {
  return carouselIntervalMs;
}

int consumeRemoteNightModeChange() {
  int v = pendingNightModeChange;
  pendingNightModeChange = -1;
  return v;
}

int consumeRemoteGameModeChange() {
  int v = pendingGameModeChange;
  pendingGameModeChange = -1;
  return v;
}
