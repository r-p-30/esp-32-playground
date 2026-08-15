#pragma once

void initDisplay();

// Redraws the screen for the given card index (0-based).
void showCard(int index);

// Starts the short-press sparkle overlay. Non-blocking - actual drawing
// happens in updateDisplayAnimation().
void triggerAnimation();

// Call every loop() iteration. No-op unless an animation is in flight.
void updateDisplayAnimation();
