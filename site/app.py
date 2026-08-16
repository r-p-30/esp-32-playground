import functools
import json
import os
import threading
import time

from dotenv import load_dotenv
from flask import Flask, flash, jsonify, redirect, render_template, request, session, url_for
from flask_sock import Sock

import state

load_dotenv()  # local dev only - Render sets real env vars directly

app = Flask(__name__)
app.secret_key = os.environ.get("FLASK_SECRET_KEY", "dev-only-not-secure")
sock = Sock(app)

SITE_PASSWORD = os.environ.get("SITE_PASSWORD")
DEVICE_API_KEY = os.environ.get("DEVICE_API_KEY")

DEVICE_STATE_FIELDS = list(state.DEFAULT_STATE.keys())

# Mirrors EmojiAnimFamily in Cards.h - which shortcodes share one default
# animation/toggle. "bit" matches the firmware's Card::animatedEmojiDisableMask
# bit layout exactly, so the mask computed here means the same thing there.
# Sparkle/star's "glyph" here is just a nicer label for the toggle row -
# the device draws both as real vector shapes, not these characters (see
# drawSparkleGlyph()/drawStarGlyph() in DisplayUI.cpp); the corner/text
# picker option glyphs elsewhere (dashboard.html's EMOJI_LIST) are the ones
# that matter for what shortcode actually gets stored. "star" used to be
# called "snowflake" - the bitmap (Adafruit's logo, not a real snowflake)
# reads more like a star, so it was renamed.
EMOJI_FAMILIES = [
    {"key": "heart", "label": "Heart", "glyph": "♥", "codes": [":heart:"], "bit": 0},
    {"key": "smile", "label": "Smile", "glyph": "☺", "codes": [":smile:", ":smileb:"], "bit": 1},
    {"key": "sparkle", "label": "Sparkle", "glyph": "✦", "codes": [":sparkle:"], "bit": 2},
    {"key": "diamond", "label": "Diamond", "glyph": "♦", "codes": [":diamond:"], "bit": 3},
    {"key": "notes", "label": "Notes", "glyph": "♪", "codes": [":note:", ":notes:"], "bit": 4},
    {"key": "star", "label": "Star", "glyph": "★", "codes": [":star:"], "bit": 5},
]
BORDER_FLASH_DISABLE_BIT = 6


def all_emoji_families(card):
    """Every family, annotated with whether it's currently present in this
    card's text or corner slots. Every family always gets a toggle row in
    the DOM (dashboard.html) - JS (card-preview.js's
    updateEmojiToggleVisibility()) shows/hides each row live as the text
    box or corner pickers change, so a toggle appears the instant its
    emoji is added, not only after a save/reload. "used" here is just the
    server-rendered initial state for that same visibility."""
    haystack = (card.get("text") or "") + " " + " ".join(card.get("cornerEmoji") or [])
    disable_mask = card.get("animatedEmojiDisableMask", 0)
    result = []
    for fam in EMOJI_FAMILIES:
        used = any(code in haystack for code in fam["codes"])
        result.append({**fam, "used": used, "checked": not (disable_mask & (1 << fam["bit"]))})
    return result


# ---- Device-facing API (docs/remote-api-spec.md) ----
# Auth is a shared secret header, not a login - deliberately separate from
# SITE_PASSWORD below (docs/site-project-plan.md section 5: "don't reuse
# the device key as the site's own auth").

def _device_authorized():
    return DEVICE_API_KEY and request.headers.get("X-Device-Key") == DEVICE_API_KEY


def _device_state_json():
    full_state = state.load_state()
    return json.dumps({k: full_state.get(k) for k in DEVICE_STATE_FIELDS})


@app.get("/api/state")
def get_state():
    # Manual/debug read of current state (curl-friendly) - the device
    # itself no longer polls this now that it holds a permanent WebSocket
    # connection (see /ws/device below); pushes go out on that instead.
    if not _device_authorized():
        return jsonify({"error": "unauthorized"}), 401
    return _device_state_json(), 200, {"Content-Type": "application/json"}


# One device connects at a time (personal project, single recipient) - the
# dashboard/card-manager routes below push a fresh state JSON down this
# socket the instant something changes, instead of the device having to
# poll for it. Guarded by a lock since Flask serves the WS connection and
# the HTTP action routes from different threads.
_device_ws = None
_device_ws_lock = threading.Lock()


def _push_state_to_device():
    global _device_ws
    with _device_ws_lock:
        ws = _device_ws
        if ws is None:
            return
        try:
            ws.send(_device_state_json())
        except Exception:
            _device_ws = None


@sock.route("/ws/device")
def device_ws(ws):
    global _device_ws
    if not _device_authorized():
        ws.close()
        return

    with _device_ws_lock:
        _device_ws = ws
    ws.send(_device_state_json())  # so the device has current state immediately on connect

    try:
        while True:
            message = ws.receive()
            if message is None:
                break
            # Two message types the device sends: a "cardsReport" once
            # right after connecting (authoritative resync of card
            # content - see sendCardsReport() in RemoteControl.cpp) and a
            # small periodic heartbeat otherwise (see
            # docs/remote-api-spec.md). Anything else/unparseable is
            # ignored.
            try:
                payload = json.loads(message)
            except ValueError:
                continue
            if not isinstance(payload, dict):
                continue
            if isinstance(payload.get("cardsReport"), list):
                state.import_cards_from_device(payload["cardsReport"])
            else:
                state.save_heartbeat(payload)
                # Keeps the dashboard's Night/Game mode toggles honest when
                # the physical button (not the site) is what changed them -
                # see sync_modes_from_heartbeat()'s docstring. Both fields
                # are always present in the device's heartbeat payload (see
                # sendHeartbeatIfDue() in RemoteControl.cpp), so a plain
                # bool() is enough - no missing-key case to guard against.
                if "nightMode" in payload and "gameMode" in payload:
                    state.sync_modes_from_heartbeat(bool(payload["nightMode"]), bool(payload["gameMode"]))
    finally:
        with _device_ws_lock:
            if _device_ws is ws:
                _device_ws = None


# ---- Site auth (protects the dashboard/card manager from anyone who finds
# the URL - see docs/site-project-plan.md section 5) ----

def login_required(view):
    @functools.wraps(view)
    def wrapped(*args, **kwargs):
        if not session.get("logged_in"):
            return redirect(url_for("login", next=request.path))
        return view(*args, **kwargs)
    return wrapped


@app.get("/")
def index():
    return redirect(url_for("dashboard") if session.get("logged_in") else url_for("login"))


@app.route("/login", methods=["GET", "POST"])
def login():
    error = None
    if request.method == "POST":
        if SITE_PASSWORD and request.form.get("password") == SITE_PASSWORD:
            session["logged_in"] = True
            return redirect(request.args.get("next") or url_for("dashboard"))
        error = "Wrong password."
    return render_template("login.html", error=error)


@app.post("/logout")
def logout():
    session.clear()
    return redirect(url_for("login"))


# ---- Dashboard ----
# Single page: actions/status (left), card editor (middle), live preview
# (right) - see dashboard.html. ?edit=<index> picks which card the editor
# column shows; every action route below redirects back here with that
# same param so the selected card doesn't reset after an unrelated action.

@app.get("/dashboard")
@login_required
def dashboard():
    heartbeat = state.load_heartbeat()
    last_seen_sec_ago = int(time.time()) - heartbeat["receivedAt"] if heartbeat else None

    all_cards = state.load_cards()
    edit_index = request.args.get("edit", type=int)
    if edit_index is None or not (0 <= edit_index < state.NUM_CARDS):
        edit_index = next((c["index"] for c in all_cards if not c["populated"]), state.CUSTOM_CARD_INDEX)

    return render_template(
        "dashboard.html",
        state=state.load_state(),
        heartbeat=heartbeat,
        last_seen_sec_ago=last_seen_sec_ago,
        device_connected=_device_ws is not None,
        cards=all_cards,
        edit_index=edit_index,
        custom_card_index=state.CUSTOM_CARD_INDEX,
        emoji_families=all_emoji_families(all_cards[edit_index]),
        border_flash_checked=not (all_cards[edit_index].get("animatedEmojiDisableMask", 0) & (1 << BORDER_FLASH_DISABLE_BIT)),
    )


@app.post("/dashboard/buzz")
@login_required
def action_buzz():
    state.apply_update({"buzz": True})
    _push_state_to_device()
    flash("Buzzed the device.", "success")
    return redirect(url_for("dashboard"))


@app.post("/dashboard/animation")
@login_required
def action_animation():
    state.apply_update({"triggerAnimation": True})
    _push_state_to_device()
    flash("Played the current card's animation.", "success")
    return redirect(url_for("dashboard"))


@app.post("/dashboard/identify")
@login_required
def action_identify():
    state.apply_update({"identifyPing": True})
    _push_state_to_device()
    flash("Sent identify ping.", "success")
    return redirect(url_for("dashboard"))


@app.post("/dashboard/carousel")
@login_required
def action_carousel():
    enabled = request.form.get("enabled") == "on"
    interval = int(request.form.get("intervalSec") or 8)
    state.apply_update({"carouselEnabled": enabled, "carouselIntervalSec": interval})
    _push_state_to_device()
    flash("Carousel settings saved.", "success")
    return redirect(url_for("dashboard"))


@app.post("/dashboard/night-mode")
@login_required
def action_night_mode():
    enabled = request.form.get("enabled") == "on"
    state.apply_update({"nightModeEnabled": enabled})
    _push_state_to_device()
    flash(f"Night mode {'on' if enabled else 'off'} sent.", "success")
    return redirect(url_for("dashboard"))


@app.post("/dashboard/game-mode")
@login_required
def action_game_mode():
    enabled = request.form.get("enabled") == "on"
    state.apply_update({"gameModeEnabled": enabled})
    _push_state_to_device()
    flash(f"Game mode {'on' if enabled else 'off'} sent.", "success")
    return redirect(url_for("dashboard"))


# ---- Card manager ----
# Card editing lives on the dashboard page itself (see above) - this route
# only exists so old /cards?edit=N links/bookmarks keep working.

@app.get("/cards")
@login_required
def cards():
    return redirect(url_for("dashboard", edit=request.args.get("edit", type=int)))


@app.post("/cards/<int:index>/save")
@login_required
def card_save(index):
    if not (0 <= index < state.NUM_CARDS):
        return redirect(url_for("dashboard"))

    text = request.form.get("text", "")
    align_h = request.form.get("alignH", "center")
    align_v = request.form.get("alignV", "middle")
    corner_emoji = [request.form.get(f"corner{i}", "") for i in range(4)]
    duration_ms = int(request.form.get("durationMs") or 1000)
    make_active = request.form.get("makeActive") == "on"

    # Every family's checkbox is always present in the submitted form (see
    # dashboard.html/card-preview.js - hidden rows are CSS display:none,
    # which browsers still submit), so this reads all of them regardless
    # of which were visible/used at the time.
    old_card = state.load_cards()[index]
    disable_mask = old_card.get("animatedEmojiDisableMask", 0)
    for fam in EMOJI_FAMILIES:
        field = f"animate_{fam['key']}"
        if request.form.get(field) == "on":
            disable_mask &= ~(1 << fam["bit"])
        else:
            disable_mask |= (1 << fam["bit"])

    # Border flash always has a toggle in the form (unlike the per-emoji
    # ones above), regardless of what's used on the card.
    if request.form.get("animate_border") == "on":
        disable_mask &= ~(1 << BORDER_FLASH_DISABLE_BIT)
    else:
        disable_mask |= (1 << BORDER_FLASH_DISABLE_BIT)

    update = {
        "cardTextIndex": index,
        "cardText": text,
        "cardTextAlignH": align_h,
        "cardTextAlignV": align_v,
        "cardCornerEmoji": corner_emoji,
        "cardAnimatedEmojiDisableMask": disable_mask,
        "cardAnimationDurationIndex": index,
        "cardAnimationDurationMs": duration_ms,
    }
    if make_active:
        update["showCard"] = index
    state.apply_update(update)
    state.set_card_text(index, text)
    state.set_card_layout(index, align_h, align_v, corner_emoji)
    state.set_card_animated_emoji_mask(index, disable_mask)
    state.set_card_animation_duration(index, duration_ms)
    _push_state_to_device()
    flash(f"Card {index} saved{' and made active.' if make_active else '.'}", "success")
    return redirect(url_for("dashboard", edit=index))


if __name__ == "__main__":
    # threaded=True: without it, the dev server is single-threaded and a
    # held-open /ws/device connection would block every other request.
    app.run(debug=True, threaded=True, port=int(os.environ.get("PORT", 5000)))
