# Default card text/timings, mirrored from firmware/DeskMate/Cards.cpp so
# the site's card manager shows real content on first run instead of blanks.
# This is only the *seed* for instance/cards.json (see state.py) - editing
# a card on the site never touches this file, only the persisted copy.
# Not secret; just a convenience snapshot, so it's fine to commit.

ANIMATION_DURATION_MS_DEFAULT = 1000

SEED_CARDS = [
    {"index": 0, "text": "right now", "animationDurationMs": ANIMATION_DURATION_MS_DEFAULT, "populated": True},
    {"index": 1, "text": "HAPPY\nBIRTHDAY", "animationDurationMs": 1800, "populated": True},
    {"index": 2, "text": "good morning", "animationDurationMs": ANIMATION_DURATION_MS_DEFAULT, "populated": True},
    {"index": 3, "text": "wake up and\nuse more than\n2 neurons\ntoday :)", "animationDurationMs": ANIMATION_DURATION_MS_DEFAULT, "populated": True},
    {"index": 4, "text": "chomp chomp", "animationDurationMs": 3000, "populated": True},
    {"index": 5, "text": "fool for you", "animationDurationMs": ANIMATION_DURATION_MS_DEFAULT, "populated": True},
    {"index": 6, "text": "music time !!!!", "animationDurationMs": ANIMATION_DURATION_MS_DEFAULT, "populated": True},
    {"index": 7, "text": "sudo make_me\nhappy\n\nsudo send pw\nto me: ******", "animationDurationMs": ANIMATION_DURATION_MS_DEFAULT, "populated": True},
    {"index": 8, "text": "i miss you", "animationDurationMs": ANIMATION_DURATION_MS_DEFAULT, "populated": True},
    {"index": 9, "text": "You: zero bugs\nfound in this\nfriendship.\nCVE score: 0.0", "animationDurationMs": ANIMATION_DURATION_MS_DEFAULT, "populated": True},
    {"index": 10, "text": "action. magic.\ntime bending.", "animationDurationMs": ANIMATION_DURATION_MS_DEFAULT, "populated": True},
    {"index": 11, "text": "firewall rule:\nALLOW you\nFROM anywhere\nTO my_heart", "animationDurationMs": ANIMATION_DURATION_MS_DEFAULT, "populated": True},
    {"index": 12, "text": "connect the\ndots", "animationDurationMs": ANIMATION_DURATION_MS_DEFAULT, "populated": True},
    {"index": 13, "text": "Status: 200 OK\nUptime: since\nday one.\nNo patches\nneeded.", "animationDurationMs": ANIMATION_DURATION_MS_DEFAULT, "populated": True},
    {"index": 14, "text": "here are some\nhappy thoughts", "animationDurationMs": ANIMATION_DURATION_MS_DEFAULT, "populated": True},
    {"index": 15, "text": "friendship.log", "animationDurationMs": ANIMATION_DURATION_MS_DEFAULT, "populated": True},
    {"index": 16, "text": "home is where\nyou are", "animationDurationMs": ANIMATION_DURATION_MS_DEFAULT, "populated": True},
    {"index": 17, "text": "psst... there's\nmore", "animationDurationMs": ANIMATION_DURATION_MS_DEFAULT, "populated": True},
    {"index": 18, "text": "hehe\n\n;)  xD  :P", "animationDurationMs": ANIMATION_DURATION_MS_DEFAULT, "populated": True},
    {"index": 19, "text": "how you doin'", "animationDurationMs": ANIMATION_DURATION_MS_DEFAULT, "populated": True},
    {"index": 20, "text": "shit happens!\nflush it &\nmove on", "animationDurationMs": ANIMATION_DURATION_MS_DEFAULT, "populated": True},
    {"index": 21, "text": "let it snow", "animationDurationMs": ANIMATION_DURATION_MS_DEFAULT, "populated": True},
    {"index": 22, "text": "-- empty slot --\n\nnothing here\nyet", "animationDurationMs": ANIMATION_DURATION_MS_DEFAULT, "populated": False, "custom": True},
    {"index": 23, "text": "good night", "animationDurationMs": ANIMATION_DURATION_MS_DEFAULT, "populated": True},
]

# Layout defaults - only visible on the device for cards using the generic
# ANIM_BOUNCE layout (the reserved/custom slot, marked "custom": True above)
# - see Cards.h. "center"/"middle" here matches every built-in card's fixed
# hand-drawn layout, so this is a safe no-op default for the other 22.
for _card in SEED_CARDS:
    _card.setdefault("alignH", "center")
    _card.setdefault("alignV", "middle")
    _card.setdefault("cornerEmoji", ["", "", "", ""])  # [topLeft, topRight, bottomLeft, bottomRight]
    _card.setdefault("animatedEmojiDisableMask", 0)  # 0 = every emoji family animates by default

# The reserved/custom slot's index, read off whichever SEED_CARDS entry is
# tagged "custom": True above instead of a hardcoded number - so reordering
# cards[] in firmware/DeskMate/Cards.cpp (and mirroring that reorder here)
# just means moving the "custom": True tag along with the entry, not hunting
# down every place that assumed it stays at a fixed index.
CUSTOM_CARD_INDEX = next(c["index"] for c in SEED_CARDS if c.get("custom"))
