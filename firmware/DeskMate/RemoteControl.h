#pragma once

// Holds one permanent WebSocket connection to the hosted site (see
// docs/remote-api-spec.md) instead of the old battery-conscious
// poll/disconnect cycle - the device is USB-powered now, so WiFi and this
// socket both just stay up for the device's entire runtime. The server
// pushes a fresh state JSON the instant something changes on the site
// (buzz, card edit, etc.) - no polling delay. Call beginRemoteControl()
// once after WiFi connects (see DeskMate.ino's setup()), then
// loopRemoteControl() every loop() iteration - both are safe no-ops if
// RemoteApi.h still has its placeholder URL.
void beginRemoteControl();

// Pumps the WebSocket connection (processes incoming pushes, handles
// reconnects) and sends a small periodic heartbeat over the same socket
// so the site can show "last seen" (REMOTE_HEARTBEAT_INTERVAL_MS in
// Config.h). Cheap and non-blocking - call every loop() iteration
// regardless of WiFi state.
void loopRemoteControl(int currentCard, bool nightModeActive, bool inGameMode);

// Returns the card index to jump to if a new remote update arrived since
// the last call, otherwise -1. Each pending jump is returned exactly once,
// then cleared - it's a one-time nudge, not a lock on the knob.
int consumeRemoteCardJump();

// Returns true exactly once if a new remote update requested a buzz.
bool consumeRemoteBuzz();

// Returns true exactly once if a new remote update requested the current
// card's short-press animation, without anyone touching the button.
bool consumeRemoteAnimationTrigger();

// Returns true exactly once if a new remote update requested the
// "identify" ping - a way to confirm an update reached the device
// without touching any card content.
bool consumeIdentifyPing();

// Carousel state - reflects the *current* value from the last pushed
// update (not revision-gated like the actions above, since this is an
// ongoing setting rather than a one-time event).
bool isCarouselEnabled();
unsigned long getCarouselIntervalMs();

// Unlike carousel (which has no local competing control), night mode and
// game mode can also be toggled by the physical button - so these are
// edge-triggered on the *site's* value actually changing, not blindly
// reasserted every push. That way a local toggle sticks until the site
// genuinely changes its stored value again, instead of being fought by a
// default that never moved. Returns 1 (on), 0 (off), or -1 (no change
// since last call).
int consumeRemoteNightModeChange();
int consumeRemoteGameModeChange();
