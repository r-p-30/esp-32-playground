#pragma once

// Polls the hosted state endpoint (see docs/remote-api-spec.md) on
// a bounded interval (REMOTE_POLL_INTERVAL_MS in Config.h), and - if
// RemoteApi.h's heartbeat URL is configured - POSTs a small status
// heartbeat in the same WiFi session. Safe to call every loop()
// iteration - internally rate-limited, and never blocks or breaks the
// core card-cycling experience if WiFi or the endpoint is unavailable
// (matches the plan's offline-first requirement). The three parameters
// are only used to build the heartbeat body.
void pollRemoteControl(int currentCard, bool nightModeActive, bool inGameMode);

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

// Carousel state - reflects the *current* value from the last successful
// poll (not revision-gated like the actions above, since this is an
// ongoing setting rather than a one-time event).
bool isCarouselEnabled();
unsigned long getCarouselIntervalMs();

// Returns true exactly once each time a randomly-scheduled "ambient"
// notification comes due (only when randomNotifyEnabled is set remotely).
// Call every loop() iteration - internally self-scheduling.
bool consumeRandomNotify();

// Unlike carousel/randomNotify (which have no local competing control),
// night mode and game mode can also be toggled by the physical button -
// so these are edge-triggered on the *site's* value actually changing,
// not blindly reasserted every poll. That way a local toggle sticks
// until the site genuinely changes its stored value again, instead of
// being fought every ~60s by a default that never moved. Returns 1 (on),
// 0 (off), or -1 (no change since last call).
int consumeRemoteNightModeChange();
int consumeRemoteGameModeChange();
