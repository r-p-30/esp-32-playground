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

from cards_seed import CUSTOM_CARD_INDEX, SEED_CARDS

INSTANCE_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "instance")
STATE_PATH = os.path.join(INSTANCE_DIR, "state.json")
CARDS_PATH = os.path.join(INSTANCE_DIR, "cards.json")
HEARTBEAT_PATH = os.path.join(INSTANCE_DIR, "heartbeat.json")

NUM_CARDS = len(SEED_CARDS)

# Fields the device only acts on when `revision` is new (docs/remote-api-spec.md).
ONE_SHOT_NEUTRAL = {
    "showCard": None,
    "activeGame": None,
    "cardTextIndex": None,
    "cardText": None,
    "cardTextAlignH": None,
    "cardTextAlignV": None,
    "cardCornerEmoji": None,
    "cardAnimatedEmojiDisableMask": None,
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


def import_cards_from_device(cards_report):
    """Overwrites the local card cache with what the device just reported
    (sent once on every WebSocket connect - see sendCardsReport() in
    RemoteControl.cpp). The device's RAM only ever changes via a server
    push, so it's a more reliable source of truth than this cache, which
    a redeploy/idle restart on free hosting can silently reset to the
    cards_seed.py defaults - see the module docstring above."""
    cards = load_cards()
    by_index = {c["index"]: c for c in cards}
    for entry in cards_report:
        index = entry.get("index")
        if index not in by_index:
            continue
        by_index[index].update({
            "text": entry.get("text", by_index[index]["text"]),
            "animationDurationMs": entry.get("animationDurationMs", by_index[index]["animationDurationMs"]),
            "populated": entry.get("populated", by_index[index]["populated"]),
            "alignH": entry.get("alignH", by_index[index]["alignH"]),
            "alignV": entry.get("alignV", by_index[index]["alignV"]),
            "cornerEmoji": entry.get("cornerEmoji", by_index[index]["cornerEmoji"]),
            "animatedEmojiDisableMask": entry.get(
                "animatedEmojiDisableMask", by_index[index].get("animatedEmojiDisableMask", 0)),
        })
    save_cards(cards)


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


def set_card_animated_emoji_mask(index, disable_mask):
    cards = load_cards()
    cards[index]["animatedEmojiDisableMask"] = disable_mask
    save_cards(cards)


def set_card_layout(index, align_h, align_v, corner_emoji):
    """corner_emoji: list of 4 shortcode strings (or "" for none), in
    [topLeft, topRight, bottomLeft, bottomRight] order - only visible on
    the device for cards using the generic ANIM_BOUNCE layout (i.e. the
    reserved/custom card), see Cards.h."""
    cards = load_cards()
    cards[index]["alignH"] = align_h
    cards[index]["alignV"] = align_v
    cards[index]["cornerEmoji"] = corner_emoji
    save_cards(cards)


def sync_modes_from_heartbeat(night_mode, game_mode):
    """Keeps nightModeEnabled/gameModeEnabled in sync with what the device
    actually reports on every heartbeat - night mode and game mode can
    both be triggered by the physical button, not just the site's toggle,
    so without this the dashboard checkbox would only reflect the truth
    when the site was the one that changed it (see docs/remote-api-spec.md
    "device -> site" heartbeat fields). Deliberately NOT apply_update():
    that clears every one-shot field (buzz, showCard, etc.) and bumps
    revision on every call, which would risk clobbering an action the
    dashboard just sent that the device hasn't processed yet, given a
    heartbeat can land in between. This only ever touches these two
    continuous fields."""
    st = load_state()
    changed = st.get("nightModeEnabled") != night_mode or st.get("gameModeEnabled") != game_mode
    if not changed:
        return
    st["nightModeEnabled"] = night_mode
    st["gameModeEnabled"] = game_mode
    save_state(st)


def load_heartbeat():
    return _read_json(HEARTBEAT_PATH, None)


def save_heartbeat(payload):
    payload = dict(payload)
    payload["receivedAt"] = int(time.time())
    _write_json(HEARTBEAT_PATH, payload)
