#pragma once

enum EncoderEvent {
  ENC_NONE,
  ENC_NEXT,
  ENC_PREV,
  ENC_SHORT_PRESS,
  ENC_LONG_PRESS
};

void initEncoder();

// Call every loop() iteration. Returns at most one event per call.
EncoderEvent pollEncoder();
