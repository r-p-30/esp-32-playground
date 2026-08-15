import functools
import os
import time

from dotenv import load_dotenv
from flask import Flask, jsonify, redirect, render_template, request, session, url_for

import state

load_dotenv()  # local dev only - Render sets real env vars directly

app = Flask(__name__)
app.secret_key = os.environ.get("FLASK_SECRET_KEY", "dev-only-not-secure")

SITE_PASSWORD = os.environ.get("SITE_PASSWORD")
DEVICE_API_KEY = os.environ.get("DEVICE_API_KEY")

DEVICE_STATE_FIELDS = list(state.DEFAULT_STATE.keys())


# ---- Device-facing API (docs/remote-api-spec.md) ----
# Auth is a shared secret header, not a login - deliberately separate from
# SITE_PASSWORD below (docs/site-project-plan.md section 5: "don't reuse
# the device key as the site's own auth").

def _device_authorized():
    return DEVICE_API_KEY and request.headers.get("X-Device-Key") == DEVICE_API_KEY


@app.get("/api/state")
def get_state():
    if not _device_authorized():
        return jsonify({"error": "unauthorized"}), 401
    full_state = state.load_state()
    return jsonify({k: full_state.get(k) for k in DEVICE_STATE_FIELDS})


@app.post("/api/heartbeat")
def post_heartbeat():
    if not _device_authorized():
        return jsonify({"error": "unauthorized"}), 401
    payload = request.get_json(silent=True) or {}
    state.save_heartbeat(payload)
    return jsonify({"ok": True})


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

@app.get("/dashboard")
@login_required
def dashboard():
    heartbeat = state.load_heartbeat()
    last_seen_sec_ago = int(time.time()) - heartbeat["receivedAt"] if heartbeat else None
    return render_template(
        "dashboard.html",
        state=state.load_state(),
        heartbeat=heartbeat,
        last_seen_sec_ago=last_seen_sec_ago,
    )


@app.post("/dashboard/buzz")
@login_required
def action_buzz():
    state.apply_update({"buzz": True})
    return redirect(url_for("dashboard"))


@app.post("/dashboard/animation")
@login_required
def action_animation():
    state.apply_update({"triggerAnimation": True})
    return redirect(url_for("dashboard"))


@app.post("/dashboard/identify")
@login_required
def action_identify():
    state.apply_update({"identifyPing": True})
    return redirect(url_for("dashboard"))


@app.post("/dashboard/carousel")
@login_required
def action_carousel():
    enabled = request.form.get("enabled") == "on"
    interval = int(request.form.get("intervalSec") or 8)
    state.apply_update({"carouselEnabled": enabled, "carouselIntervalSec": interval})
    return redirect(url_for("dashboard"))


@app.post("/dashboard/random-notify")
@login_required
def action_random_notify():
    enabled = request.form.get("enabled") == "on"
    min_sec = int(request.form.get("minSec") or 60)
    max_sec = int(request.form.get("maxSec") or 300)
    state.apply_update({
        "randomNotifyEnabled": enabled,
        "randomNotifyMinSec": min_sec,
        "randomNotifyMaxSec": max_sec,
    })
    return redirect(url_for("dashboard"))


@app.post("/dashboard/night-mode/<value>")
@login_required
def action_night_mode(value):
    state.apply_update({"nightModeEnabled": value == "on"})
    return redirect(url_for("dashboard"))


@app.post("/dashboard/game-mode/<value>")
@login_required
def action_game_mode(value):
    state.apply_update({"gameModeEnabled": value == "on"})
    return redirect(url_for("dashboard"))


# ---- Card manager ----

@app.get("/cards")
@login_required
def cards():
    return render_template("cards.html", cards=state.load_cards())


@app.post("/cards/<int:index>/text")
@login_required
def card_set_text(index):
    if not (0 <= index < state.NUM_CARDS):
        return redirect(url_for("cards"))
    text = request.form.get("text", "")
    state.apply_update({"cardTextIndex": index, "cardText": text})
    state.set_card_text(index, text)
    return redirect(url_for("cards"))


@app.post("/cards/<int:index>/activate")
@login_required
def card_activate(index):
    if not (0 <= index < state.NUM_CARDS):
        return redirect(url_for("cards"))
    state.apply_update({"showCard": index})
    return redirect(url_for("cards"))


@app.post("/cards/<int:index>/duration")
@login_required
def card_set_duration(index):
    if not (0 <= index < state.NUM_CARDS):
        return redirect(url_for("cards"))
    duration_ms = int(request.form.get("durationMs") or 1000)
    state.apply_update({"cardAnimationDurationIndex": index, "cardAnimationDurationMs": duration_ms})
    state.set_card_animation_duration(index, duration_ms)
    return redirect(url_for("cards"))


if __name__ == "__main__":
    app.run(debug=True, port=int(os.environ.get("PORT", 5000)))
