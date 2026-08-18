#pragma once

#include "WifiCredentials.h"  // gitignored - see WifiCredentials.example.h

// ---- Pin assignments (matches wiring table in desk-mate-project-plan.md) ----
#define PIN_OLED_SDA   21
#define PIN_OLED_SCL   22
#define PIN_BUZZER     25
#define PIN_ENC_CLK    18
#define PIN_ENC_DT     19
#define PIN_ENC_SW     23

// ---- OLED ----
#define OLED_WIDTH     128
#define OLED_HEIGHT    64
#define OLED_I2C_ADDR  0x3C

// ---- Encoder / button timing ----
#define LONG_PRESS_MS          500
#define VERY_LONG_PRESS_MS     2000
#define ANIMATION_DURATION_MS  1000

// ---- WiFi / NTP (Phase 1) ----
// How long the boot-time fast path (reconnect via NVS-stored credentials -
// see connectWiFiAtBootWithSetupFallback() in WifiUtil.cpp) waits before
// giving up and falling to the setup portal below. WiFi stays connected
// for the device's entire runtime after this (USB-powered, no battery
// reason to drop it) - see DeskMate.ino and RemoteControl.cpp.
#define WIFI_CONNECT_TIMEOUT_MS  5000UL

// ---- WiFi setup portal (Phase 2 - WiFiManager) ----
// Only runs at boot, and only if the normal fast connect (above) fails.
// Bounded so a device that's never configured (or whose WiFi stopped
// working) still finishes booting into the offline card browser instead
// of hanging - see WifiUtil.cpp. 4 minutes - real-world testing showed
// 2 minutes was too tight once you count phone captive-portal detection
// and typing a password once you've joined the setup AP.
// WIFI_SETUP_AP_NAME / WIFI_SETUP_AP_PASSWORD live in WifiCredentials.h
// (gitignored).
#define WIFI_SETUP_PORTAL_TIMEOUT_SEC  240UL

#define NTP_SERVER          "pool.ntp.org"
#define GMT_OFFSET_SEC       (5 * 3600 + 1800)  // TODO: adjust for your timezone (IST default)
#define DAYLIGHT_OFFSET_SEC  0

// ---- Game mode (hidden dino-jump easter egg) ----
// Redraws/physics steps deliberately slow (~10fps) - a faster loop made
// the jump window too tight to react to on real hardware. The jump arc
// itself is time-based (millis()), not tied to this interval, so it stays
// smooth regardless.
#define GAME_FRAME_INTERVAL_MS    100UL
#define GAME_COUNTDOWN_PHASE_MS   700UL   // Ready/Set/Go, 3 phases = ~2.1s total
#define GAME_JUMP_DURATION_MS     750UL
// Jump height auto-picks one of these based on which cactus is active when
// you press (see GameEngine.cpp's gameJump()) - a single fixed arc either
// clips the tall cactus or floats needlessly high over the short one.
// Both leave a clear visual gap over the obstacle's height (not just
// barely exceeding it) - the shrunk dino sprite in DisplayUI.cpp is small
// enough that even the big-cactus peak (42) stays on-screen, just a couple
// px shy of the top border (DisplayUI.cpp's GAME_GROUND_Y sits low enough
// now - see its own comment - that there's room to spare here).
// BIG bumped up from 36: height at the moment of overlap is
// peak * 4*t*(1-t) where t is how far into the 750ms jump you are when the
// cactus's x crosses the hitbox - only right at t=0.5 do you actually see
// the peak, so a press that's a beat early/late in the jump (not just a
// jump that's too short/late relative to the obstacle) saw meaningfully
// less than 36px of actual clearance, especially for the wider/taller big
// cactus - a taller peak scales height up at every t, not just the apex.
#define GAME_JUMP_PEAK_SMALL_PX   28
#define GAME_JUMP_PEAK_BIG_PX     42
#define GAME_JUMP_FORWARD_PX      14
#define GAME_SCROLL_SPEED_PX_S    45.0f
// Grace subtracted from the cactus height a jump needs to clear - a
// rotary-encoder button press can't be timed as precisely as a mouse
// click, so a jump that's a few px short of the obstacle's literal top
// still counts as cleared, rather than punishing normal input jitter.
#define GAME_JUMP_CLEARANCE_FORGIVENESS_PX  6

// A run ends after this many hits, not the first one - see GameEngine.cpp's
// stepGame() collision handling. A hit that doesn't end the run just
// dismisses that obstacle (same as scrolling fully off) and play continues.
#define GAME_LIVES  3
// Once the last life is lost, GameEngine.cpp holds GAME_OVER_FLASH for this
// long (frozen play-scene, DisplayUI.cpp blinks a "GAME OVER" box over it)
// before handing off to the resting GAME_OVER score card - kept under 2s so
// it reads as a quick beat, not a wait. BLINK_MS is the on/off toggle
// period within that window (DisplayUI-side, free-running off millis() -
// doesn't need to know when the flash actually started).
#define GAME_OVER_FLASH_DURATION_MS  1500UL
#define GAME_OVER_FLASH_BLINK_MS     250UL

// ---- Whack-a-mole ----
// Hole count around the on-screen circle - first thing to tune down (e.g.
// to 6) if 8 turns out too hard given the rotate-to-aim input; kept as a
// single constant for exactly that reason.
#define MOLE_POSITIONS      8
// Starting per-mole time window - generous, since aiming requires
// rotating to the right position first ("spot it, rotate to it, press").
// Shrinks as score grows (see the three constants below), rather than
// staying fixed for the whole run.
#define MOLE_VISIBLE_MS     1800UL
// Every this-many points, the visible window shrinks by MOLE_VISIBLE_MS_STEP
// - a fixed score interval rather than per-hit, so the ramp is predictable
// (score 20 -> one step down, score 40 -> two steps down, ...).
#define MOLE_DIFFICULTY_STEP_SCORE  20
#define MOLE_VISIBLE_MS_STEP        150UL
// Floor on the shrinking window - stays reachable given the rotate-then-
// press input even at high score, same "keep it forgiving" reasoning as
// Dino's optional speed-up (GAME_SCROLL_SPEED_PX_S's comment elsewhere).
#define MOLE_VISIBLE_MS_MIN         700UL
// Run ends after this many timed-out (unhit) moles - a miss on this game
// means a mole's window expired, not a whiffed press on an empty hole.
#define MOLE_MAX_MISSES     10

// ---- Snake ----
// 8px cells - matches the pip/segment sizes already used elsewhere in game
// mode. Playfield sits below a permanent HUD strip (score + life pips),
// unlike Whack's corner-overlay HUD - Snake's playfield can occupy any
// cell on screen (Whack's ring keeps its corners clear by construction),
// so score text needs a reserved strip instead of just tucking into a
// corner.
#define SNAKE_CELL_PX             8
#define SNAKE_HUD_HEIGHT_PX       8
#define SNAKE_GRID_COLS           16   // 128 / SNAKE_CELL_PX
#define SNAKE_GRID_ROWS           7    // (64 - SNAKE_HUD_HEIGHT_PX) / SNAKE_CELL_PX

#define SNAKE_START_LENGTH        3

// Fixed-cadence auto-advance (a real grid-step tick, not a per-frame move)
// - see currentTickIntervalMs() in SnakeEngine.cpp. Base pace is a touch
// slower than the first pass (was 220) - still "keep it forgiving" per
// GAME_SCROLL_SPEED_PX_S's reasoning, just not very much slower.
// Speeds up as the body grows, same shape as Whack-a-mole's difficulty
// ramp (MOLE_DIFFICULTY_STEP_SCORE/MOLE_VISIBLE_MS_STEP/MOLE_VISIBLE_MS_MIN
// above) - every SNAKE_SPEEDUP_STEP_SCORE points, cut the interval by
// SNAKE_SPEEDUP_STEP_MS, floored at SNAKE_TICK_MIN_MS. Unlike Whack's wide
// 1800->700 range spread over many small 150ms steps, Snake's whole usable
// range is much narrower (a rotary-encoder-steered snake can't go much
// below ~130ms and stay reactable-to), so a 150ms cut is nearly the entire
// range in one step - in practice this reads as one clear speed-up once
// the body has grown a bit (10 food eaten), then holds at the faster pace,
// rather than the old barely-perceptible 10ms-every-5-points creep.
#define SNAKE_TICK_INTERVAL_MS    260UL
#define SNAKE_TICK_MIN_MS         130UL
#define SNAKE_SPEEDUP_STEP_SCORE  10
#define SNAKE_SPEEDUP_STEP_MS     150UL

// A hit on one of the last N tail segments doesn't count as a collision -
// see SnakeEngine.cpp's stepSnakeGame() collision check. First constant to
// tune down/up if self-collision ends up feeling too forgiving or too
// strict.
#define SNAKE_TAIL_FORGIVENESS_SEGMENTS  2

// "3 retries" reuses GAME_LIVES (above, shared with Dino) rather than a
// separate constant - that's exactly what GAME_LIVES already means.

// If a hit happens within this many ticks of the last turn (0 = the same
// tick the turn was taken), the turn - not just that final step - is
// judged responsible for the crash: retreatSnake() rewinds all the way
// back to exactly how the snake looked right before that turn (position
// *and* heading), instead of just nudging back a couple of cells. A hit
// with no turn this recently just gets the plain small nudge below,
// heading unchanged - see SnakeEngine.cpp's stepSnakeGame()/retreatSnake().
#define SNAKE_TURN_HIT_WINDOW_TICKS  3

// How far back (in cells) the snake is nudged after a non-fatal hit that
// *isn't* within SNAKE_TURN_HIT_WINDOW_TICKS of a turn - opposite whichever
// direction it was heading at the moment of the hit. Kept small (one or
// two cells) on purpose: just enough to clear the crash site, not a real
// do-over - see SnakeEngine.cpp's retreatSnake().
#define SNAKE_HIT_RETREAT_CELLS      2
// Frozen "HIT!" flash before the retreat + recountdown - shorter than
// GAME_OVER_FLASH_DURATION_MS since play resumes right after this, it
// isn't the end of the run. Blink cadence reuses GAME_OVER_FLASH_BLINK_MS,
// no separate constant needed.
#define SNAKE_HIT_FLASH_DURATION_MS  900UL
// Per-number duration for the post-hit 3/2/1 recountdown (numbers only, no
// "READY/SET/GO" words) - snappier than GAME_COUNTDOWN_PHASE_MS (700ms)
// since there's no word to read, just a number, and the player already
// knows how to play by this point in the run.
#define SNAKE_RECOUNTDOWN_PHASE_MS   500UL

// ---- Remote control (optional - only does anything once RemoteApi.h has
// real values in it; harmless no-op otherwise) ----
// The device holds one permanent WebSocket connection to the hosted site
// (see RemoteControl.cpp) - state changes push instantly, no polling
// delay. This just controls how often a small "I'm alive" status message
// goes out over that same connection, purely for the site's "last seen"
// display - doesn't affect how fast buzzes/card changes/etc. arrive.
#define REMOTE_HEARTBEAT_INTERVAL_MS  30000UL
