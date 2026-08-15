#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <string.h>
#include <stdio.h>
#include "RemoteControl.h"
#include "WifiUtil.h"
#include "Cards.h"
#include "Config.h"
#include "RemoteApi.h"

static unsigned long lastPollAtMs = 0;
static bool everPolled = false;
static long lastAppliedRevision = -1;

static int pendingCardJump = -1;
static bool pendingBuzz = false;
static bool pendingAnimationTrigger = false;
static bool pendingIdentify = false;

// Continuous state (not revision-gated - always reflects the latest poll).
static bool carouselEnabled = false;
static unsigned long carouselIntervalMs = 5000;
static unsigned long carouselStartedAtMs = 0;
static bool carouselWasEnabledLastPoll = false;

// Hard local safety cap - the carousel keeps the display + logic active
// continuously, which adds up on battery. Regardless of what the site
// says, it auto-stops after this long; the site has to explicitly set
// carouselEnabled false then true again (a real stop/restart) to get
// another window, rather than it running forever unattended.
#define CAROUSEL_MAX_RUNTIME_MS (60UL * 60UL * 1000UL)

static bool randomNotifyEnabled = false;
static unsigned long randomNotifyMinMs = 60000;
static unsigned long randomNotifyMaxMs = 300000;
static unsigned long nextRandomNotifyAtMs = 0;

static bool nightModeRawLastPoll = false;
static bool gameModeRawLastPoll = false;
static int pendingNightModeChange = -1;  // -1 none, 0 off, 1 on
static int pendingGameModeChange = -1;

// Expects "https://host/path" form.
static bool splitUrl(const char* urlStr, String& outHost, String& outPath) {
  String url = urlStr;
  if (!url.startsWith("https://")) return false;

  int hostStart = 8;
  int pathStart = url.indexOf('/', hostStart);
  outHost = (pathStart == -1) ? url.substring(hostStart) : url.substring(hostStart, pathStart);
  outPath = (pathStart == -1) ? "/" : url.substring(pathStart);
  return outHost.length() > 0;
}

static bool fetchRemoteState(String& outBody) {
  String host, path;
  if (!splitUrl(REMOTE_API_URL, host, path)) return false;

  WiFiClientSecure client;
  client.setInsecure();  // reading device-key-gated JSON, not trusting the cert chain
  client.setTimeout(5000);

  if (!client.connect(host.c_str(), 443)) return false;

  client.print(String("GET ") + path + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "X-Device-Key: " + REMOTE_API_KEY + "\r\n" +
               "Connection: close\r\n\r\n");

  unsigned long start = millis();

  // Skip past response headers.
  while (client.connected() && (millis() - start) < 5000) {
    String line = client.readStringUntil('\n');
    if (line == "\r") break;
  }

  outBody = "";
  while ((client.connected() || client.available()) && (millis() - start) < 5000) {
    while (client.available()) {
      outBody += (char)client.read();
    }
  }
  client.stop();
  return outBody.length() > 0;
}

// Fire-and-forget status ping - failure here is silent and harmless, this
// is purely so the site can show "last seen" instead of guessing whether
// the device is even reachable. Doesn't affect anything else the device
// does either way.
static void sendHeartbeat(int currentCard, bool nightModeActive, bool inGameMode) {
  String host, path;
  if (!splitUrl(REMOTE_HEARTBEAT_URL, host, path)) return;

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(5000);

  if (!client.connect(host.c_str(), 443)) return;

  char body[128];
  snprintf(body, sizeof(body),
           "{\"currentCard\":%d,\"nightMode\":%s,\"gameMode\":%s,\"uptimeSec\":%lu}",
           currentCard,
           nightModeActive ? "true" : "false",
           inGameMode ? "true" : "false",
           millis() / 1000UL);
  size_t bodyLen = strlen(body);

  client.print(String("POST ") + path + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "X-Device-Key: " + REMOTE_API_KEY + "\r\n" +
               "Content-Type: application/json\r\n" +
               "Content-Length: " + String(bodyLen) + "\r\n" +
               "Connection: close\r\n\r\n" +
               body);

  // Don't need the response - just drain briefly so the socket closes cleanly.
  unsigned long start = millis();
  while (client.connected() && (millis() - start) < 3000) {
    while (client.available()) client.read();
  }
  client.stop();
}

// RemoteApi.example.h's placeholder values - if these haven't been
// changed, there's no real endpoint configured yet, so skip entirely.
// Without this, every interval would still do a real WiFi connect +
// failed HTTPS attempt against a nonexistent host, blocking loop() (and
// the encoder along with it) for several seconds for no benefit.
static bool remoteApiConfigured() {
  return strcmp(REMOTE_API_URL, "https://your-site.example/api/state") != 0;
}

static bool heartbeatConfigured() {
  return strcmp(REMOTE_HEARTBEAT_URL, "https://your-site.example/api/heartbeat") != 0;
}

void pollRemoteControl(int currentCard, bool nightModeActive, bool inGameMode) {
  if (!remoteApiConfigured() && !heartbeatConfigured()) return;

  unsigned long now = millis();
  if (everPolled && (now - lastPollAtMs) < REMOTE_POLL_INTERVAL_MS) {
    return;
  }
  lastPollAtMs = now;
  everPolled = true;

  if (!connectWiFiBriefly(WIFI_CONNECT_TIMEOUT_MS)) {
    return;  // offline - try again next interval, core UX unaffected
  }

  if (remoteApiConfigured()) {
    String body;
    if (fetchRemoteState(body)) {
      StaticJsonDocument<512> doc;
      if (deserializeJson(doc, body) == DeserializationError::Ok) {
        // Continuous config - applied from every successful poll,
        // independent of whether revision changed (ongoing settings, not events).
        bool rawCarouselEnabled = doc["carouselEnabled"] | false;
        if (rawCarouselEnabled && !carouselWasEnabledLastPoll) {
          // False -> true transition: a genuine (re)start, begin a fresh window.
          carouselStartedAtMs = now;
        }
        carouselWasEnabledLastPoll = rawCarouselEnabled;
        carouselEnabled = rawCarouselEnabled;

        long carouselSec = doc["carouselIntervalSec"] | 5L;
        if (carouselSec < 1) carouselSec = 1;
        carouselIntervalMs = (unsigned long)carouselSec * 1000UL;

        randomNotifyEnabled = doc["randomNotifyEnabled"] | false;
        long minSec = doc["randomNotifyMinSec"] | 60L;
        long maxSec = doc["randomNotifyMaxSec"] | 300L;
        if (minSec < 1) minSec = 1;
        if (maxSec < minSec) maxSec = minSec;
        randomNotifyMinMs = (unsigned long)minSec * 1000UL;
        randomNotifyMaxMs = (unsigned long)maxSec * 1000UL;

        // Night/game mode: edge-triggered on the site's value changing,
        // not blindly reasserted - see RemoteControl.h for why.
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

          if (!doc["cardText"].isNull() && !doc["cardTextIndex"].isNull()) {
            setCardText(doc["cardTextIndex"].as<int>(), doc["cardText"].as<const char*>());
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
    }
  }

  if (heartbeatConfigured()) {
    sendHeartbeat(currentCard, nightModeActive, inGameMode);
  }

  disconnectWiFi();
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

static void scheduleNextRandomNotify(unsigned long now) {
  unsigned long span = (randomNotifyMaxMs > randomNotifyMinMs) ? (randomNotifyMaxMs - randomNotifyMinMs) : 0;
  unsigned long delay = randomNotifyMinMs + (span > 0 ? (unsigned long)random((long)span) : 0);
  nextRandomNotifyAtMs = now + delay;
}

bool consumeRandomNotify() {
  if (!randomNotifyEnabled) {
    nextRandomNotifyAtMs = 0;  // so re-enabling later picks a fresh random delay
    return false;
  }

  unsigned long now = millis();
  if (nextRandomNotifyAtMs == 0) {
    scheduleNextRandomNotify(now);
    return false;
  }
  if (now >= nextRandomNotifyAtMs) {
    scheduleNextRandomNotify(now);
    return true;
  }
  return false;
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
