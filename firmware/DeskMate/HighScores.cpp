#include <Preferences.h>
#include "HighScores.h"

// One shared NVS namespace for every game's high score, keyed by the
// short string each caller passes in (e.g. "dino", "whack") - simpler than
// a namespace per game for what's only ever a couple of small ints.
static const char* HIGHSCORE_NAMESPACE = "highscores";

int loadHighScore(const char* key) {
  Preferences prefs;
  prefs.begin(HIGHSCORE_NAMESPACE, /*readOnly=*/true);
  int value = prefs.getInt(key, 0);
  prefs.end();
  return value;
}

void saveHighScore(const char* key, int score) {
  Preferences prefs;
  prefs.begin(HIGHSCORE_NAMESPACE, /*readOnly=*/false);
  prefs.putInt(key, score);
  prefs.end();
}
