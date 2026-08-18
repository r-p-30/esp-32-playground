# Game Mode — Actual Implementation

**Hardware context:** ESP32, 128x64 I2C OLED (SSD1306, address 0x3C), KY-040 rotary encoder (CLK→GPIO18, DT→GPIO19, SW→GPIO23, quadrature-decoded), passive piezo buzzer (GPIO25). Single rotation axis + single button as the only inputs — no d-pad, no multi-button combos.

This doc describes what's actually built (`firmware/DeskMate/Game.h`, `GameEngine.*`, `WhackEngine.*`, `SnakeEngine.*`, `TetrisEngine.*`, `RacingEngine.*`, `Games.cpp`) — code is the source of truth if the two ever disagree again. §1–5 (Dino Jump through Car Racing) are playable today and match picker-menu order exactly. §6–7 (Tank Battle, Tank Arena) are planned next; Tank Battle occupies the picker menu's still-reserved final slot (`GAME_SLOT_6` in `Game.h`, shown as "Tank Battle (soon)"), Tank Arena has no slot at all yet — both distilled from `docs/brick-game-plan.md`'s finalized-but-unbuilt spec.

**Entry/navigation:** long-press from the card browser opens the game picker menu (not a boot-time hidden mode). Rotate to browse titles (`GAMES[]` in `Games.cpp`), short press launches the highlighted one. From inside a game, long/very-long press backs out one level to the picker menu; from the picker menu, one further level out to the card browser. The companion site's dashboard can also toggle game mode remotely, or jump straight into a specific game, bypassing the picker menu.

**Shared lifecycle** every game follows (`Game.h`'s contract): `COUNTDOWN` ("Ready"/"Set"/"Go", ~700ms/phase) → `RUNNING` → on a fatal hit, a frozen `OVER_FLASH` beat (1.5s blink + a sustained alarm tone) → resting `OVER` score card, where a short press restarts back to a fresh countdown. Each game keeps its own best score, loaded lazily from NVS flash on first entry and written back only when a run actually beats it (`HighScores.h`) — so it survives reboots/reflashes, not just RAM. Redraw/step is throttled to roughly 10fps (`GAME_FRAME_INTERVAL_MS` = 100ms) as a baseline, but each game's actual gameplay tick (jump arc, mole timer, grid-step) runs on its own `millis()`-based timing independent of that throttle, so pacing stays consistent regardless of how often `step()` fires.

---

## 1. Dino Jump

**Concept:** single-lane runner — a cactus scrolls in from the right at a fixed speed, player times a jump to clear it. Only one obstacle is ever in play at a time (scrolls fully off, then a randomized gap, then the next spawns) — a deliberate pacing choice so each one stays clearly anticipatable.

**Input:**
- Press = jump. Rotation is unused (no-op) — this game has nothing to steer.

**Core mechanics:**
- Cactus scroll speed is fixed (45px/s); gap between obstacles is randomized 60–110px.
- Jump is a fixed 750ms arc (parabola for height, half-sine for a forward nudge) — not held-button-controlled, a single press commits to the whole arc.
- Jump peak height auto-picks based on whichever cactus is currently active when you press: 28px for the small variant, 42px for the big one (a single fixed arc would either clip the big cactus or float needlessly over the small one). Both cactus sizes appear at random per spawn.
- 3 lives shared with every other game via `GAME_LIVES`. A hit dismisses the cactus immediately (same as it scrolling off) and costs one life; the run only ends on the 3rd hit. A near-miss on timing still counts as cleared via a small clearance-forgiveness margin, so it isn't punished as harshly as a mouse-click-precision game would be.
- Score increases continuously with scroll distance while running (not "+1 per obstacle jumped").

**Feedback:** a jump chirp on press, a distinct miss beep on a non-fatal hit, and a sustained alarm + blink on the fatal 3rd hit.

---

## 2. Whack-a-Mole

**Concept:** a single mole marker appears at one of several fixed holes arranged in a circle on screen. Player aims and whacks it before its window runs out.

**Layout:** 8 holes (`MOLE_POSITIONS`) arranged in a circle — a natural fit for a rotary knob, since rotating sweeps the cursor around them in order.

**Input:**
- Rotate = move the cursor one hole per detent, wrapping around the circle.
- Press = whack at the cursor's current hole.

**Core mechanics:**
- Mole spawns at a random hole, never the same one twice in a row.
- Visible window starts at 1800ms and shrinks by 150ms every 20 points scored, floored at 700ms — generous at first since aiming needs a rotate-then-press sequence, not instant reflexes.
- A hit (cursor on the mole's hole at the moment of the press) scores a point and the mole immediately respawns elsewhere. A press while the cursor is on an empty hole is simply ignored — it does **not** count as a miss.
- A miss is specifically a mole's window expiring unhit. The run ends after 10 misses (`MOLE_MAX_MISSES`), not on the first one.

**Feedback:** a hit reuses the same high-pitched chirp as Dino's jump; a timed-out mole plays a low miss beep; the 10th miss plays the alarm + blink.

---

## 3. Snake

**Grid:** 16 columns × 7 rows of 8px cells, with an 8px HUD strip reserved at the top of the screen for score (unlike Whack's corner overlay, Snake's playfield can use any cell including the corners, so score needs its own reserved strip).

**Input:**
- Rotate = queue a turn relative to the current heading (clockwise/right or counter-clockwise/left, not an absolute compass direction — avoids needing 4 distinct rotation amounts). Only the most recent queued turn before the next tick is kept, so two quick opposite turns can't sneak in an effective reversal between ticks.
- Press = pause/resume while running; on the game-over screen, press restarts.

**Core mechanics:**
- Movement is a fixed-cadence grid tick, not continuous: starts at 260ms/tick, speeds up by 150ms every 10 points eaten, floored at 130ms — one clear speed-up once the snake has grown a bit, then it holds at the faster pace.
- Food spawns at a random empty cell; eating it grows the snake by one segment and increments score.
- **Screen wrap-around on all four edges** (Nokia Snake style) — going off an edge is explicitly not a death condition.
- Self-collision is the only death condition, softened by two forgiveness mechanisms:
  - The last 2 tail segments are excluded from the self-collision check entirely (a hit there is ignored outright).
  - 3 lives, shared with every other game via `GAME_LIVES`. A non-fatal self-hit freezes the world with a "HIT!" flash (900ms), then the snake is retreated — either fully rewound to exactly how it looked right before its most recent turn (if that turn happened within the last 3 ticks, since the turn itself is judged responsible for the crash), or just nudged back 2 cells opposite its current heading (if not) — followed by a quick numbers-only 3/2/1 recountdown (500ms/phase, no "Ready/Set/Go" words since the player already knows how to play by then) before resuming. The 3rd hit ends the run instead of retreating.

**Feedback:** eating food reuses the jump chirp; a non-fatal hit plays the miss beep; the fatal hit plays the alarm + blink.

---

## 4. Tetris

**Board:** 10 columns × 20 rows of 6px cells in game-space (row = fall axis, column = the axis the encoder shifts). The display renders this rotated 90° so the fall axis reads along the screen's physical 128px edge — a render-only transform in `DisplayUI.cpp`; the engine's own row/column math never rotates.

**Pieces:** the 7 standard tetrominoes (I, O, T, S, Z, J, L) plus one deliberate non-standard extra — a bare 1×1 `SINGLE` block. Each spawns centered at the top of the board, piece type picked at random.

**Input:**
- Rotate = shift the falling piece one column left/right per detent. Rejected outright (no partial move) if it would collide with a wall or a locked cell.
- Press = rotate the piece 90° clockwise, via plain box rotation with no SRS wall-kick attempts. Rejected outright if the rotated shape would collide. The `SINGLE` block explicitly ignores rotate — a 1×1 block has no visually distinct rotation state, so treating it as a no-op reads correctly instead of looking like the block hopping sideways.

**Core mechanics:**
- Falling is a fixed-cadence tick, not continuous: 500ms/row normally, dropping to 250ms/row once the piece's lowest cell crosses the board's vertical midpoint (recomputed per piece — every new piece starts slow again). A successful shift or rotate resets the fall timer, giving a full fresh interval to keep maneuvering before the next forced drop — uncapped, since a rotary encoder can't be spammed the way a keyboard soft-drop cheese would need.
- A piece locks when it can't move down further. Completed rows clear (a single backward pass handles clearing multiple non-contiguous rows in one lock), and rows above shift down to fill the gap.
- Score per lock depends on how many lines cleared simultaneously: 10 / 30 / 50 / 80 points for 1 / 2 / 3 / 4 lines — a small multi-line bonus, not full classic Tetris scoring.
- The run ends when a freshly spawned piece doesn't fit (topout) — there's no lives/misses concept here, a blocked spawn ends the run outright rather than costing one of several chances.

**Feedback:** a line-clear beep whose tone scales with how many lines cleared at once; topout plays the alarm + blink.

---

## 5. Car Racing

**Concept:** a vertically-scrolling road, rendered with the same 90°-rotated axis convention as Tetris (row = approach axis, reading along the screen's physical 128px edge; column = the lane axis the encoder shifts). The player's car sits fixed at the near edge; obstacle cars spawn at the far edge and advance one row per tick, and the player switches lanes to dodge them.

**Input:**
- Rotate = shift the player's car one lane per detent, clamped at the outer lanes (no wraparound, unlike Snake's grid).
- Press = pause/resume while running; on the game-over screen, press restarts.

**Core mechanics:**
- Fixed at 3 lanes for the whole run, from the very first countdown — no widening mid-run. Road width is 60px, evenly split 20px/lane.
- Obstacle lane is picked independently at random on every spawn, not excluded from lanes already in use — lanes can double up, or one can sit empty for a few spawns in a row; that's real randomness, not a bug.
- Advance tick starts at 240ms/row, holds flat until score reaches 30, then steps down by 20ms every 20 points from there (not a uniform ramp from zero), floored at 110ms.
- **Single life, instant game over on collision** — unlike Dino/Snake/Whack's multi-hit forgiveness, there's no miss allowance here; the first obstacle reaching the player's row while sharing its lane ends the run immediately.

**Feedback:** the alarm + blink on collision, same as every other game's fatal ending.

---

## 6. Tank Battle *(coming soon — not yet implemented)*

Occupies the game menu's still-reserved final slot (`GAME_SLOT_6` in `Game.h`, shown as "Tank Battle (soon)" in the picker). Behavior is fully resolved in `docs/brick-game-plan.md`, just not built yet:

- Same 90°-rotated approach-axis convention as Tetris/Racing: player's tank sits at the near edge in one of 4 fixed lanes (set at the start of a run, no widening ramp like Racing's), enemy tanks spawn at the far edge and advance down their spawn lane.
- Rotate = shift lanes, clamped (no wraparound). Press = fire a bullet from the current lane — fully committed to firing, no pause-on-press.
- One bullet in flight at a time; a bullet's lane is locked at the moment it's fired, independent of later lane changes. At most one enemy active per lane, with a shared minimum spawn-gap across all lanes (same fairness mechanism as Racing's).
- Single life, instant game over — an enemy reaching the player's row or colliding with the tank ends the run immediately, matching Racing's collision rule rather than Dino/Snake's multi-hit forgiveness.
- Score increments only on a kill. Enemy spawn rate/speed ramps up with score, same forgiving step-with-a-floor shape as every other game's difficulty ramp. Best score will persist to NVS like every other game here.

**Feedback (planned):** a beep on firing, a distinct tone on a kill, the shared game-over alarm on death.

---

## 7. Tank Arena *(coming soon — deferred, working title)*

Documented for future reference in `docs/brick-game-plan.md`, but explicitly **not scheduled** yet and has no game-menu slot at all today. Free-roaming top-down tank arena — unlike Tank Battle's fixed-lane model, the player's tank can be anywhere on screen and face/move in all 4 directions, with 2–3 enemy tanks active at fixed spawn positions/facings. Best built once a second encoder is added to the hardware, since the current single-encoder input budget (rotate + short-press only, long/very-long press are hardwired to always exit to the game menu) can't cleanly give this game three independent actions (turn, move, fire) without compromise — a one-encoder fallback design (auto-advance in the tank's current facing, same continuous movement model as Snake) is sketched in the plan doc if it ends up built before a second encoder does.
