# Desk Mate — Companion Site Plan

**Purpose:** a small hosted site that lets you remotely control the device wherever it's set up — change what's on screen, edit card text, add new cards into the reserved slots, and trigger sounds/animations — without reflashing. This is the "Phase 2" config site from `desk-mate-project-plan.md`, fleshed out now that the device-side contract (`remote-api-spec.md`) is fully built. Built mobile-first as a PWA (§9) so it works well from a phone, since it's meant to be driven remotely as much as in person.

The device holds a permanent WebSocket connection to this site (it's USB-powered, so there's no battery reason to disconnect) and reacts the instant the site pushes a new state - no polling delay. The site's only job is to push the right JSON and let you edit it through a UI instead of hand-writing JSON.

---

## 1. Feature List (mapped to what the device already supports)

| Feature | Backed by | Notes |
|---|---|---|
| Change which card is showing | `showCard` | One-time nudge — you can still turn the knob after |
| Edit an existing card's text | `cardTextIndex` + `cardText` | Any index except 20 (see below) |
| Add a new card | `cardTextIndex` + `cardText` targeting 20 | Starts as "-- empty slot --"; only 1 exists, no reflash needed to fill it |
| Tune an animation's timing | `cardAnimationDurationIndex` + `cardAnimationDurationMs` | For testing - no effect on the clock/equalizer cards (no press-animation) |
| Buzz the device | `buzz` | Distinct 3-beep pattern, separate from the routine navigation beep |
| Play the current card's animation | `triggerAnimation` | Same animation as a short press, no button needed |
| Carousel / auto-advance | `carouselEnabled` + `carouselIntervalSec` | Continuous setting, not a one-time action. **Device enforces a hard 1-hour auto-stop** regardless of what's sent — see below |
| Emoji in card text | `:heart:` `:smile:` `:smileb:` `:spade:` `:club:` `:diamond:` `:note:` `:notes:` shortcodes | Only these 8 render correctly — see constraint below |
| Text alignment on the custom card | `cardTextAlignH` + `cardTextAlignV` | Only visible on cards using the generic bounce layout (the reserved slot by default) - see remote-api-spec.md |
| Corner-emoji decorations on the custom card | `cardCornerEmoji` (array of 4) | Same bounce-layout-only caveat as alignment |
| "Is this thing on?" check | `identifyPing` | Beeps twice + flashes the screen, touches nothing else — useful while building the site |
| Night mode (dim screen + big clock) | `nightModeEnabled` | Also toggleable on-device via a 5-second press-hold. **Edge-triggered, not sticky** — see the gotcha in §3 |
| Game mode (placeholder for now) | `gameModeEnabled` | Also toggleable on-device via a 2-second press-hold. Real gameplay isn't built — this just switches to a "coming soon" screen |

Full field-level contract: `docs/remote-api-spec.md`. This doc is about the site *around* that contract, not the contract itself.

---

## 2. The one hardware constraint that shapes the UI

The OLED can't render real Unicode emoji — only 8 specific symbol glyphs (old IBM CP437 codepage: hearts, suits, smileys, music notes). **The emoji picker in the UI should only offer these 8**, not a general emoji keyboard — anything else silently renders as garbage on the actual device with no error surfaced back to the site. This is worth enforcing at the UI level (a fixed row of 8 buttons that insert the shortcode), not just documented and hoped for.

---

## 3. Data model

The site needs to persist one "current state" blob — this is what gets pushed as JSON over the WebSocket connection whenever it changes (and once immediately on connect):

```
{
  revision: number,          // bump on every save
  showCard: number | null,
  cardTextIndex: number | null,
  cardText: string | null,
  cardTextAlignH: string | null,   // "left" | "center" | "right"
  cardTextAlignV: string | null,   // "top" | "middle" | "bottom"
  cardCornerEmoji: string[] | null,  // [topLeft, topRight, bottomLeft, bottomRight] shortcodes
  cardAnimationDurationIndex: number | null,
  cardAnimationDurationMs: number | null,
  buzz: boolean,
  triggerAnimation: boolean,
  identifyPing: boolean,
  carouselEnabled: boolean,
  carouselIntervalSec: number,
  nightModeEnabled: boolean,
  gameModeEnabled: boolean,
}
```

Plus, separately (for the UI's own use, not sent to the device): the site should keep its **own copy of all 21 content cards' current text**, so the card manager can show "what's on the device right now" rather than a blank form. The device is the source of truth for what's actually displaying, but the site can't read that back (one-directional, see §6) — so the site's copy is really "what we last told it," which is a reasonable approximation as long as nobody edits cards by any other means.

**Card order isn't fixed** — it's just array order in `firmware/DeskMate/Cards.cpp`, and it's already been reordered once during development. The reserved empty slot (index 20 as of this writing) isn't guaranteed to sit at the end — don't assume "reserved slots are the last N indices" anywhere in the site's logic. Pull the current order from `Cards.cpp` directly when building the card manager rather than hardcoding indices from this doc.

### Important gotcha: one-shot fields need to be cleared, not left sticky

`showCard`, `cardTextIndex`/`cardText`, `buzz`, and `triggerAnimation` only fire on the device when `revision` is new — but if your backend leaves `buzz: true` sitting in the saved state after the "buzz now" button was clicked, the **next unrelated edit** (e.g. changing a card's text) will bump `revision` again and cause an unintended second buzz, since the device just sees "new revision, buzz is true."

**Design rule for your backend:** every save should explicitly set the one-shot fields to their neutral value (`null`/`false`) *unless* that specific action is what the user just triggered. Don't accumulate old actions across saves. The continuous settings (`carouselEnabled`, `carouselIntervalSec`, `nightModeEnabled`, `gameModeEnabled`) are the exception — those should persist normally.

### Second gotcha: night/game mode toggles reflect "last sent," not "current state"

`nightModeEnabled` and `gameModeEnabled` are different from every other continuous field, because the *physical button* can also change them. The device only reacts when your saved value actually **changes** between pushes — so if you save `nightModeEnabled: true` once and someone later long-presses to turn it off locally, the device correctly stays off even though your database still says `true`. Toggling it on again remotely requires writing a genuinely new value.

Practically: don't build a toggle switch that claims to show "is night mode on right now" based on what you last saved — it can't know that (see §6). Either label it as "send night mode on/off" (an action, not a status), or use the heartbeat's `nightMode`/`gameMode` fields to show actual current state separately from the control itself.

---

## 4. Pages / UI

**Dashboard**
- Quick actions: "Buzz now", "Play animation now", "Identify" (the last one is for you while building/debugging, not really a day-to-day feature) — each just fires a save with only that one field set
- Carousel toggle + interval input
- Night mode / game mode: send-only buttons ("turn on" / "turn off"), not a toggle that claims to reflect current state — see the gotcha in §3
- Status line fed by the connection + heartbeat (see §6): "● Connected"/"● Not connected", "last seen 2 min ago", current card, current mode — this is the one place in the UI that shows *actual* device state instead of *last sent* state
- Every action shows a flash-message confirmation on save - "no way to know if it saved" was real feedback from an earlier version of this UI

**Card manager** (built as a single-page builder, not a long stacked list - see below)
- A compact numbered picker for all 22 cards (`?edit=<index>` in the URL) - clicking one loads it into the builder without leaving the page
- Text box + the 8-shortcode emoji row for inserting into the text
- Text alignment (H/V) and 4 corner-emoji pickers - only visible on the device for cards using the generic bounce layout (the reserved slot by default), documented as such in the UI so it's not a surprise when a built-in animated card ignores them
- A live canvas preview (128×64, scaled up) next to the fields that updates as you type/change settings, before you save - catches clipped/overflowing text and bad alignment choices before they reach the real device
- "Save" is one combined action (text + alignment + corner emoji + animation duration together, one revision bump) and "make active" is separate - not a pile of per-field save buttons
- Desktop: fields on the left, live preview on the right, sized to fit without scrolling the page. Mobile: stacks vertically - fields/buttons first, emoji pickers next, live preview last

---

## 5. Security

- **Site-level auth**: the device's `X-Device-Key` only stops the *device* from being hit by strangers — it does nothing to stop a random visitor from finding your site's URL and editing what the device shows. If the site is publicly reachable, put a basic password/login in front of it. If it's only ever accessed by you, an obscure unlisted URL is a lower-effort (but weaker) alternative.
- **Don't reuse the device key as the site's own auth** — keep them as two separate secrets, so leaking one doesn't compromise the other.
- Match whatever `REMOTE_API_KEY` you put in the device's `RemoteApi.h` (gitignored, never commit it) — the site needs to check for this exact value on the WebSocket handshake, and reject anything else.

---

## 6. Heartbeat: closing the one-directional gap

Everything else in this doc is site → device. Without a heartbeat, the site has no way to know whether the device is even online, what card is *actually* showing (only what was last sent), or whether a night/game mode toggle actually landed.

This is now built: the device holds one permanent WebSocket connection (`wss://.../ws/device`, `REMOTE_API_URL` in `RemoteApi.h`) instead of the old poll/disconnect model, and sends a small status message back over that same connection every `REMOTE_HEARTBEAT_INTERVAL_MS` (`Config.h`, default 30s):

```json
{"currentCard": 0, "nightMode": false, "gameMode": false, "uptimeSec": 184320}
```

Your site's WebSocket handler (checked against the same `X-Device-Key` header at connect time) just needs to parse any inbound message as this shape and save it with a timestamp — that timestamp is what "last seen 2 min ago" is built from. Since it's a held-open connection rather than a periodic request, the site can also show "connected right now" directly, independent of the heartbeat content. The device doesn't expect a reply to the heartbeat; treat it as fire-and-forget. Full contract in `remote-api-spec.md`'s Heartbeat section.

This is worth building early rather than skipping to the nicer UI — it's what makes the night/game mode gotcha in §3 usable at all (otherwise you're toggling blind).

---

## 7. Hosting options (pick based on how much backend you want to write)

1. **Static JSON smoke test** — no backend at all, hand-edit a JSON file hosted somewhere with a stable raw URL (e.g. a GitHub Gist). Proves the device-side contract works end-to-end. No UI, no card manager — just validates plumbing.
2. **Minimal backend** — a small app (Node/Express, Python/Flask, or a serverless function) with one GET endpoint (serves state) and a basic HTML form that POSTs updates. This is probably the real target for v1 — small enough to build in an evening, real enough to actually use.
3. **Nicer UI** — same backend, but with the card manager grid + OLED preview + emoji picker described in §4.

Phase 2/3 here might be a good one to build collaboratively later, per the original plan.

---

## 8. Can the site run locally on a phone instead of being hosted?

Short answer: technically yes, but there's a real snag, and I'd recommend against it for this project specifically.

**The snag:** the device firmware only speaks secure WebSocket (`wss://`, via `WebSocketsClient::beginSSL()`), not plain `ws://`. Getting a phone to serve real, working TLS locally means generating and trusting a certificate, which is genuinely fiddly even for a developer — not a "just run a script" thing. Plain `ws://` (no TLS at all) would be much easier to stand up locally, but the firmware doesn't support it right now, and switching would mean the shared-secret header travels unencrypted — an acceptable tradeoff on a private home/hotspot network, not something I'd want to silently ship as the default.

**If you still want this specifically:** the realistic path is running Termux (a Linux terminal app for Android) with a small Python/Node WebSocket server, and the ESP32 pointed at that phone's local IP. On a phone's own hotspot, the phone itself is usually reachable at a predictable address (Android hotspots commonly use `192.168.43.1`), which helps. This needs `beginSSL()` swapped for the library's plain `begin()` (or made to support both) — a real but contained code change, not something I'd do speculatively without confirming you want to go this route.

**What I'd actually recommend instead:** a real (still free-tier) hosted backend — Render, Railway, Fly.io, a small Vercel/Cloudflare function, or even a Google Apps Script Web App (free, zero server management, HTTPS/WSS built in). This sidesteps the local-hosting problem entirely, works regardless of which WiFi network the device is on at any given moment (relevant if it isn't always on the same network you are), and doesn't require a phone to be running a background server the whole time the device is on. The "local app" instinct makes sense for keeping this self-contained and not dependent on a third party, but a free-tier host achieves the same independence without the TLS problem.

---

## 9. Mobile experience: build it as a PWA (decided)

Controlling the device from a phone should feel like an app, not "open a browser and navigate to a URL" — but a real native app (Kotlin/Java or Flutter/React Native, separate codebase, app-store or sideload distribution) is a lot of overhead for what's fundamentally a handful of buttons and a card list. The middle ground: a **Progressive Web App**.

- Build the site mobile-first / responsive from the start — cheap now, annoying to retrofit later.
- Add a `manifest.json` (app name, icon, theme color, `"display": "standalone"`) linked from the page `<head>`.
- That alone is enough for "Add to Home Screen" on Android Chrome and iOS Safari to create a real home-screen icon that opens full-screen with no browser address bar — for this use case, indistinguishable from a native app.
- A service worker (for offline asset caching) isn't really needed — this app's whole point is live device control, there's nothing meaningful to use offline. Skip it for v1; revisit only if reopen speed becomes annoying.
- Needs a couple of icon sizes (192×192 and 512×512 are the common ones) — trivial to generate from one source image.

This is the concrete answer to "should this be an app" from the earlier local-hosting discussion in §8 — a PWA gets the home-screen/full-screen feel without a separate codebase, app-store account, or the local-TLS problem that a real native+local-server setup would still have to solve.

---

## 10. Open decisions

- Whether the site needs its own login now or can stay an obscure URL for now (low stakes while it's just for personal use)
- Whether local-on-phone hosting (§8) is still worth pursuing given the PWA decision covers the "feels like an app" goal without it — probably not, but flagging since it was raised
- Icon/branding for the PWA manifest — needs at least a simple logo
