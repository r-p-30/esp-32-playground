#pragma once

// Holds one permanent WebSocket connection to the hosted site (see
// docs/remote-api-spec.md) instead of the old battery-conscious
// poll/disconnect cycle - the device is USB-powered now, so WiFi and this
// socket both just stay up for the device's entire runtime. The server
// pushes a fresh state JSON the instant something changes on the site
// (buzz, card edit, etc.) - no polling delay.
//
// The actual socket I/O runs on its own FreeRTOS task pinned to the
// second core (see RemoteControl.cpp) - a flaky connection can make
// WebSocketsClient block for a long time (in particular the TCP/TLS
// connect on every reconnect attempt), and that used to stall this whole
// call, which meant a bad remote connection could freeze the
// encoder/game/buzzer along with it. Now that task can block all it wants
// without touching anything input-driven; only a small mutex-guarded
// mailbox crosses between the two tasks. None of that is visible from
// this header - beginRemoteControl()/loopRemoteControl() are called
// exactly the same way as before this split.
//
// Call beginRemoteControl() once after WiFi connects (see DeskMate.ino's
// setup()), then loopRemoteControl() every loop() iteration - both are
// safe no-ops if RemoteApi.h still has its placeholder URL.
void beginRemoteControl();

// Drains anything the network task received since the last call (applying
// state pushes, resyncing the card report on a fresh connect) and queues
// a periodic heartbeat for it to send (REMOTE_HEARTBEAT_INTERVAL_MS in
// Config.h so the site can show "last seen"). Cheap and non-blocking -
// call every loop() iteration regardless of WiFi state; this call itself
// never touches the socket directly, so it can't be the thing that blocks
// (see the task-split note above). activeGame is whatever GAMES[] index
// is currently selected (Game.h) - reported so the site can highlight
// which game is playing right now, same reasoning as currentCard for
// cards.
void loopRemoteControl(int currentCard, bool nightModeActive, bool inGameMode, int activeGame);

// Returns the card index to jump to if a new remote update arrived since
// the last call, otherwise -1. Each pending jump is returned exactly once,
// then cleared - it's a one-time nudge, not a lock on the knob.
int consumeRemoteCardJump();

// Returns the GAMES[] index (Game.h) to jump straight into if a new remote
// update requested one, otherwise -1 - same one-time-nudge shape as
// consumeRemoteCardJump() above, but for games["activeGame"] instead of
// showCard. Unlike consumeRemoteGameModeChange() below, this launches a
// specific game directly rather than just opening the picker menu, and
// fires on every new selection regardless of whether game mode was already
// on - that's what lets the site swap the active game mid-run.
int consumeRemoteGameSelect();

// Returns the index of a card whose content (text/alignment/corner
// emoji/animation mask) was just updated by a remote push, otherwise -1.
// Editing a card from the site only sends `showCard` (and thus only
// triggers consumeRemoteCardJump() above) when "make active" was
// checked - saving edits to whatever card is *already* on screen doesn't
// touch showCard at all, so without this the device keeps displaying the
// stale already-rendered frame until you navigate away and back. The
// caller (DeskMate.ino) redraws only if this index matches the
// currently-shown card; a different card's content having changed is a
// no-op visually, same as before.
int consumeRemoteContentUpdate();

// Returns true exactly once if a new remote update requested a buzz.
bool consumeRemoteBuzz();

// Returns true exactly once if a new remote update requested the current
// card's short-press animation, without anyone touching the button.
bool consumeRemoteAnimationTrigger();

// Returns true exactly once if a new remote update requested the
// "identify" ping - a way to confirm an update reached the device
// without touching any card content.
bool consumeIdentifyPing();

// Forces the next loopRemoteControl() call to send a heartbeat
// immediately, bypassing REMOTE_HEARTBEAT_INTERVAL_MS - call this right
// after night mode or game mode changes locally (button press), so the
// site's stored toggle state catches up within one loop iteration instead
// of waiting up to REMOTE_HEARTBEAT_INTERVAL_MS for the next periodic
// heartbeat. Harmless no-op if the WebSocket isn't connected.
void forceHeartbeatNow();

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
