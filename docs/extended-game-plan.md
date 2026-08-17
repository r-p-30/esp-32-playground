# Game Mode Specs — ESP32 Gift Project

**Hardware context:** ESP32, 128x64 I2C OLED (SSD1306, address 0x3C), KY-040 rotary encoder (CLK→GPIO18, DT→GPIO19, SW→GPIO23, quadrature-decoded), passive piezo buzzer (GPIO25). Single rotation axis + single button as the only inputs — no d-pad, no multi-button combos.

Games are accessed via a menu (rotate to browse titles, press to select) that sits inside the existing "hidden game mode" (triggered by holding the encoder button ~5 sec at boot, separate from the main card-cycling gift mode).

---

## Snake

**Concept:** Classic snake — a growing line of segments moves around a grid, player steers it to eat food without hitting itself or the wall.

**Grid:** Divide the 128x64 screen into a coarse grid (cell size large enough to be visible on a small OLED — think ~8x8px cells, giving roughly 16x8 playable cells). Exact cell size and grid dimensions are an implementation choice, not fixed here.

**Input mapping:**
- Snake moves continuously in its current direction on a fixed tick (auto-advance, not input-driven)
- Rotating the encoder clockwise/counter-clockwise changes the snake's next direction (typically constrained to relative left/right turns rather than absolute compass directions, to avoid needing 4 distinct rotation amounts)
- Press = pause/resume, and also used on the game-over screen to return to the game menu

**Core mechanics:**
- Food spawns at a random empty grid cell
- Snake grows by one segment each time it eats food
- **Screen wrap-around (Nokia Snake II style):** moving off one edge of the grid brings the snake back in from the opposite edge, rather than ending the game. Wall collision is NOT a death condition.
- Collision with itself is the only game-over condition
- Score = number of food items eaten, shown on game-over screen
- Speed may optionally increase slightly as score grows, but should stay forgiving — this hardware's input isn't built for fast reflexes

**Feedback:** short buzzer beep on eating food, distinct (lower/longer) tone on game over.

---

## Whack-a-Mole

**Concept:** A "mole" (single visible marker) appears at one of several fixed positions arranged around/across the screen. Player aims and hits it before it disappears.

**Layout:** Divide the screen into a small number of discrete positions (e.g., 6-8 slots, arranged in a row, grid, or circle — implementation's choice, circle arrangement fits a rotary knob nicely since rotating naturally "sweeps" across positions in order).

**Input mapping:**
- Rotating the encoder moves a cursor/highlight across the discrete positions, one position per detent (using the already-solved quadrature decoding, no free continuous movement)
- Press = "whack" at the cursor's current position

**Core mechanics:**
- Mole appears at a random position for a limited time window (e.g., a second or two, tune for what's actually playable given the rotate-to-aim input, not real-time reflex)
- If the player's cursor is on the mole's position when they press, it's a hit — score increments, mole immediately respawns elsewhere
- If the time window expires without a hit, mole disappears (miss, no score change) and respawns elsewhere
- Optional: a fixed number of rounds or a countdown timer for a defined game length, ending in a final score screen, rather than playing forever

**Feedback:** short high-pitched beep on hit, short low/flat beep or no sound on miss.

**Key design note:** timing windows must be tuned generously since aiming requires rotating to the right position first — this isn't a instant-reaction game, it's "spot it, rotate to it, press," so the mole's visible duration should account for that full sequence.

---

## Tetris (simplified for this input scheme)

**Concept:** Standard falling-block puzzle, adapted since there's no separate left/right/rotate buttons — only one rotation axis and one button.

**Control scheme (this is the key adaptation, not standard Tetris controls):**
- Rotating the encoder clockwise/counter-clockwise moves the falling piece left/right, one column per detent
- Press = rotates the piece 90°
- No manual soft-drop or hard-drop control — piece falls automatically on a fixed timer tick (drop speed may increase slightly as lines clear, kept forgiving)

**Grid:** A narrower-than-standard playfield fits better on a 128x64 screen — exact column/row count is an implementation choice (standard Tetris is 10 wide; something narrower, e.g. 6-8 columns, will likely render more legibly at usable block sizes on this screen).

**Piece set:** Standard tetromino shapes (I, O, T, S, Z, J, L) — can start with a reduced subset (e.g., skip S/Z or simplify rotation states) if full rotation-state handling proves to be too much scope, and expand later.

**Core mechanics:**
- Piece spawns at top, falls each tick
- Player repositions via rotation (left/right) and rotates the piece via press, before it lands
- Piece locks in place when it can't move down further
- Completed horizontal lines clear, rows above shift down, score increments
- Game ends when a new piece can't spawn (stack reaches the top)

**Feedback:** short beep on line clear (longer/more distinct for multi-line clears if implemented), distinct tone on game over.

**Honest scope note:** this is the most complex of the three to implement correctly (piece rotation states, collision detection against the stack, line-clear detection) and the input scheme is a genuine compromise versus real Tetris controls — expect it to feel slower-paced than a standard Tetris, which is an acceptable trade-off given the hardware, not a bug to fix.

---

## Shared notes across all three

- All three plug into the same menu system: rotate to browse game titles, press to select, and (convention to decide during implementation) some consistent way to exit back to the menu (e.g., long-press) from within a game or its game-over screen
- Score/game-over screens should be simple — text is enough, no need for elaborate graphics
- Reuse the already-built and tested encoder quadrature-decoding + short/long-press detection code as the input layer for all three games, rather than re-deriving input handling per game