# Persistence for the device state the device polls, plus the site's own
# copy of card text and the last heartbeat. Backed by plain JSON files
# under instance/ (Flask convention) - fine for a single-device personal
# project, not meant to scale past that.
#
# NOTE: on Render's free tier, local disk does not survive a restart/
# redeploy (the instance spins down after ~15 min idle and comes back
# clean). instance/*.json will reset to the seed values when that happens.
# Fine while proving the contract out; if losing edited card text after an
# idle period becomes annoying, move this to a real DB or a paid disk.

import json
import os
import time

from cards_seed import SEED_CARDS

INSTANCE_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "instance")
STATE_PATH = os.path.join(INSTANCE_DIR, "state.json")
CARDS_PATH = os.path.join(INSTANCE_DIR, "cards.json")
HEARTBEAT_PATH = os.path.join(INSTANCE_DIR, "heartbeat.json")

NUM_CARDS = len(SEED_CARDS)

# Fields the device only acts on when `revision` is new (docs/remote-api-spec.md).
ONE_SHOT_NEUTRAL = {
    "showCard": None,
    "cardTextIndex": None,
    "cardText": None,
    "buzz": False,
    "triggerAnimation": False,
    "identifyPing": False,
    "cardAnimationDurationIndex": None,
    "cardAnimationDurationMs": None,
}

# Fields applied from every successful poll, independent of `revision`.
CONTINUOUS_DEFAULTS = {
    "carouselEnabled": False,
    "carouselIntervalSec": 8,
    "randomNotifyEnabled": False,
    "randomNotifyMinSec": 60,
    "randomNotifyMaxSec": 300,
    "nightModeEnabled": False,
    "gameModeEnabled": False,
}

DEFAULT_STATE = {"revision": 0, **ONE_SHOT_NEUTRAL, **CONTINUOUS_DEFAULTS}


def _ensure_instance_dir():
    os.makedirs(INSTANCE_DIR, exist_ok=True)


def _read_json(path, default):
    if not os.path.exists(path):
        return default
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def _write_json(path, data):
    _ensure_instance_dir()
    tmp_path = path + ".tmp"
    with open(tmp_path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)
    os.replace(tmp_path, path)


def load_state():
    return _read_json(STATE_PATH, dict(DEFAULT_STATE))


def save_state(state):
    _write_json(STATE_PATH, state)


def apply_update(fields):
    """Merge `fields` into the saved state, clearing every one-shot field to
    its neutral value first so nothing from a previous action re-fires
    (see the "one-shot fields need to be cleared" gotcha in
    docs/site-project-plan.md). Always bumps revision - harmless when only
    continuous fields changed, since the one-shot fields stay neutral."""
    state = load_state()
    state.update(ONE_SHOT_NEUTRAL)
    state.update(fields)
    state["revision"] = int(time.time())
    save_state(state)
    return state


def load_cards():
    return _read_json(CARDS_PATH, [dict(c) for c in SEED_CARDS])


def save_cards(cards):
    _write_json(CARDS_PATH, cards)


def set_card_text(index, text):
    cards = load_cards()
    cards[index]["text"] = text
    cards[index]["populated"] = True
    save_cards(cards)


def set_card_animation_duration(index, duration_ms):
    cards = load_cards()
    cards[index]["animationDurationMs"] = duration_ms
    save_cards(cards)


def load_heartbeat():
    return _read_json(HEARTBEAT_PATH, None)


def save_heartbeat(payload):
    payload = dict(payload)
    payload["receivedAt"] = int(time.time())
    _write_json(HEARTBEAT_PATH, payload)
