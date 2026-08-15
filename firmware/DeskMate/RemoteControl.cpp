#include <Arduino.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <string.h>
#include <stdio.h>
#include "RemoteControl.h"
#include "Cards.h"
#include "Config.h"
#include "RemoteApi.h"

static WebSocketsClient webSocket;
static bool wsStarted = false;
static unsigned long lastHeartbeatSentMs = 0;

static long lastAppliedRevision = -1;

static int pendingCardJump = -1;
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

static void applyStateJson(const uint8_t* payload, size_t length) {
  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, payload, length) != DeserializationError::Ok) return;

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
  // blindly reasserted - see RemoteControl.h for why.
  bool rawNight = doc["nightModeEnabled"] | false;
  if (rawNight != nightModeRawLastPoll) {
    pendingNightModeChange = rawNight ? 1 : 0;
  }
  nightModeRawLastPoll = rawNight;

  bool rawGame = doc["gameModeEnabled"] | false;
  if (rawGame != gameModeRawLastPoll) {
    pendingGameModeChange = rawGame ? 1 : 0;
  }
  gameModeRawLastPoll = rawGame;

  // One-shot actions below - only fire on a genuinely new revision.
  long revision = doc["revision"] | -1L;
  if (revision >= 0 && revision != lastAppliedRevision) {
    lastAppliedRevision = revision;

    if (!doc["cardTextIndex"].isNull()) {
      int idx = doc["cardTextIndex"].as<int>();

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
    }
    if (!doc["showCard"].isNull()) {
      pendingCardJump = doc["showCard"].as<int>();
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

static void onWsEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_TEXT:
      applyStateJson(payload, length);
      break;
    case WStype_CONNECTED:
      Serial.println("Remote control connected.");
      break;
    case WStype_DISCONNECTED:
      Serial.println("Remote control disconnected - will auto-retry.");
      break;
    default:
      break;
  }
}

void beginRemoteControl() {
  if (wsStarted || !remoteApiConfigured()) return;

  String host, path;
  if (!splitWsUrl(REMOTE_API_URL, host, path)) return;

  static String extraHeaders = String("X-Device-Key: ") + REMOTE_API_KEY;
  webSocket.setExtraHeaders(extraHeaders.c_str());
  webSocket.onEvent(onWsEvent);
  webSocket.setReconnectInterval(5000);
  // Protocol-level ping/pong so a half-dead connection (e.g. a NAT
  // timeout that never sends a clean close) gets noticed and reconnected
  // instead of silently going stale for a permanent connection.
  webSocket.enableHeartbeat(15000, 3000, 2);
  webSocket.beginSSL(host.c_str(), 443, path.c_str());
  wsStarted = true;
}

static void sendHeartbeatIfDue(int currentCard, bool nightModeActive, bool inGameMode) {
  if (!wsStarted || !webSocket.isConnected()) return;

  unsigned long now = millis();
  if (lastHeartbeatSentMs != 0 && (now - lastHeartbeatSentMs) < REMOTE_HEARTBEAT_INTERVAL_MS) {
    return;
  }
  lastHeartbeatSentMs = now;

  char body[128];
  snprintf(body, sizeof(body),
           "{\"currentCard\":%d,\"nightMode\":%s,\"gameMode\":%s,\"uptimeSec\":%lu}",
           currentCard,
           nightModeActive ? "true" : "false",
           inGameMode ? "true" : "false",
           millis() / 1000UL);
  webSocket.sendTXT(body);
}

void loopRemoteControl(int currentCard, bool nightModeActive, bool inGameMode) {
  if (!wsStarted) return;
  webSocket.loop();
  sendHeartbeatIfDue(currentCard, nightModeActive, inGameMode);
}

int consumeRemoteCardJump() {
  int jump = pendingCardJump;
  pendingCardJump = -1;
  return jump;
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
