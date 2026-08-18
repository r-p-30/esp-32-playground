# Game Mode Specs — Tank Battle & Racing (ESP32 Gift Project)

**Hardware context:** ESP32, 128x64 I2C OLED (SSD1306, address 0x3C), KY-040 rotary encoder (CLK→GPIO18, DT→GPIO19, SW→GPIO23, quadrature-decoded), passive piezo buzzer (GPIO25). Single rotation axis + single button as the only inputs — no d-pad, no multi-button combos.

These two games are additions to the same hidden game-mode menu described in `game-specs.md` (Snake, Whack-a-Mole, Tetris) — same access pattern (rotate to browse menu, press to select), same input layer (reuse the existing tested encoder quadrature-decoding + short/long-press code), same buzzer feedback conventions.

**Source:** Both are classic modes from the "19-in-1" (or similar bundle-count) handheld "brick game" consoles — the same LCD handheld devices that popularized Tetris/Brick Puzzle as a portable toy. Referred to on these consoles as **"Tank Classic"** / **"Tank Battle"** / **"Tanks War"** and **"Racing Classic"** respectively (naming varies slightly by manufacturer). These aren't original inventions — they're well-known, simple arcade modes that shipped together on the same physical device for decades, which is part of why they're a good fit here: they were originally designed for cheap, low-resolution monochrome LCD hardware with minimal buttons, very similar constraints to this project's OLED + single-encoder setup.

---

## Tank Battle

**Original game name:** Tank Classic / Tank Battle / Tanks War (naming varies by console manufacturer, same underlying game)

**Concept:** Player controls a tank confined to the bottom of the screen. Enemy tanks (or simple enemy markers) appear at the top and advance downward toward the player. The player moves their tank horizontally and fires bullets upward to destroy enemies before they reach the bottom or collide with the player's tank. Difficulty (enemy speed and/or spawn rate) increases as the player survives longer / scores more, matching the original console behavior where enemy speed and intelligence increase after each level.

**Screen layout (128x64 OLED):**
- Player's tank occupies a fixed row near the bottom of the screen, free to move horizontally
- Enemy tanks/markers spawn near the top and move downward over time (may also drift horizontally, depending on how much complexity is worth building — a simpler version keeps enemies in fixed lanes/columns and only moving downward)
- Player-fired bullets travel straight up from the tank's current position

**Input mapping:**
- Rotating the encoder moves the player's tank left/right, one discrete step (lane or pixel-column, implementer's choice) per detent — reuse the already-solved quadrature decoding from the encoder test code
- Press = fire a bullet upward from the tank's current horizontal position

**Core mechanics:**
- A bullet destroys an enemy on collision (enemy removed, score increments)
- An enemy reaching the player's row, or colliding with the player's tank, ends the game (alternative/simpler rule: player has a small number of lives/hits before game over, rather than instant death — implementer's choice; instant-death is closer to the original arcade feel and simpler to build)
- Enemy spawn rate and/or movement speed increases gradually as score grows, to create rising difficulty — keep the early game forgiving given the input scheme isn't built for split-second reflexes
- Only one player bullet on screen at a time is a reasonable simplification (avoids needing to track a bullet list) unless multi-bullet feels necessary once playtested

**Feedback:** short beep on firing a bullet, a distinct (higher-pitched or double-beep) tone on destroying an enemy, a longer/lower tone on game over.

---

## Racing

**Original game name:** Racing Classic / Car Race

**Concept:** A vertically-scrolling 2-lane (or narrow multi-lane) road. The player's car is fixed at the bottom of the screen; obstacle cars/objects appear at the top of the road and scroll downward toward the player, appearing to approach. The player switches lanes to avoid collisions. Speed increases progressively over time/score, matching the original console's on-screen "SPEED" and "LEVEL" indicators.

**Screen layout (128x64 OLED):**
- Vertical road bounded by two edge markers (left/right road edges), drawn as simple vertical lines or dashed borders
- Player's car sits in a fixed row near the bottom, occupying one of the available lanes (2 lanes is the simplest and truest to the original device)
- Obstacle cars spawn at the top in a random lane and scroll downward each tick
- Road may optionally scroll (dashed centerline moving downward) purely for visual effect, to reinforce the sense of forward motion — not required for the core mechanic

**Input mapping:**
- Rotating the encoder switches the player's car between lanes — with 2 lanes, this is a natural 1-detent-per-lane-switch mapping (rotate clockwise = move to right lane, counter-clockwise = move to left lane, or similar direct mapping — no need for continuous/analog positioning)
- No button action needed for core play; the press button could optionally be reserved for pause or for returning to the game menu on the game-over screen, for consistency with the other games' conventions

**Core mechanics:**
- Obstacle cars scroll downward at a fixed tick rate; when an obstacle reaches the player's row while occupying the same lane as the player, it's a collision → game over
- Score increments over time survived and/or per obstacle successfully passed
- Speed (scroll tick rate) increases gradually as score grows, mirroring the original console's explicit "SPEED"/"LEVEL" display — this game can display those two labels on-screen for authenticity if desired
- Obstacle spawn timing/frequency should be tuned so lane-switching (a single-detent rotation) is always physically achievable in time — since this is a simpler, more forgiving input than a jump-timing game, this should be the easiest of all the specced games to keep fair and fun

**Feedback:** short tick/beep as speed increases to a new level (optional, nice touch matching the "LEVEL UP" feel of the original), distinct crash tone on collision/game over.

---

## Shared notes across both

- Both plug into the same menu system as the other specced games (`game-specs.md`): rotate to browse, press to select, consistent exit-to-menu convention from game-over screens.
- Both are strong fits for this hardware specifically because their original source devices had similarly constrained input (few buttons, monochrome low-res screen) — the adaptation here is close to faithful, not a heavy compromise (unlike Tetris, which needed real control-scheme changes to work with a single rotary input).
- Racing is likely the simplest of all specced games to implement well (simplest collision logic, most forgiving input mapping) — a good candidate to build early/first if the team wants a quick win before tackling Snake or the more involved games.
- Reuse the already-tested encoder quadrature-decoding + short/long-press code as the shared input layer, consistent with all other game specs.