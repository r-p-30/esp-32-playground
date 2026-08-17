---
name: frontend-design
description: Frontend UI/CSS conventions and visual design bar for this project's companion site (site/templates, site/static). Use when adding or restyling any dashboard/card-editor/login UI — new fields, buttons, toggles, layout sections, or pages — to keep the site looking like a finished, polished product rather than a functional proof-of-concept.
---

## Design bar: polished product, not a POC

The current CSS is a functional first pass — flat single-color cards,
minimal shadow/depth, uniform tight spacing, no motion. Treat it as a
baseline to actively elevate, not a ceiling to match. Every new or
touched piece of UI should raise the bar, within the vanilla-CSS
constraint below:

- **Depth over flatness.** Give cards/panels subtle layered shadows
  (soft, low-opacity, slightly offset) instead of just a 1px border —
  borders and shadows can coexist, but a purely flat bordered box reads
  as a wireframe, not a finished surface.
- **Deliberate spacing rhythm.** Pick a spacing scale (e.g. 4/8/12/16/24/32px)
  and use it consistently instead of ad-hoc `0.35rem`/`0.4rem`/`0.6rem`
  values scattered per rule. Favor more generous whitespace around
  primary content over cramming things tight.
- **Typographic hierarchy.** Vary weight, size, and letter-spacing
  deliberately across headings/labels/body/hints — not just two font
  sizes and a muted color. Small-caps or letter-spaced uppercase labels,
  tighter heading tracking, etc. are all fair game with system fonts.
- **Motion on interaction.** Buttons, toggles, cards, and picker buttons
  should have smooth `transition`s on hover/active/focus (color, transform,
  shadow) — not just the toggle switch's existing thumb slide. A
  transform: translateY(-1px) + shadow bump on button hover, a fast
  ease on focus rings, etc.
- **Considered accent use.** The accent color is currently used flatly
  for anything "on/active." Use it more intentionally — glow/shadow tinted
  with the accent, gradient accents for primary actions, accent-tinted
  focus rings — rather than solid-fill everywhere.
- **Refined states.** Give empty states, loading states, and disabled
  states real visual treatment (not just `opacity: 0.6` dashed borders)
  — this is what separates "demo" from "product."
- **Consistent corner-radius and elevation scale** across all
  components, not per-component ad-hoc radii.

When restyling something, don't just port the existing flat style to
the new element — improve it, and consider back-porting the same
polish to the sibling components it sits next to so the page doesn't
end up visually inconsistent.

## Ground rules (still apply)

- **Vanilla only.** Plain HTML (Jinja2 templates) + one hand-written
  `style.css` + a few small single-purpose JS files. No framework, no
  CSS-in-JS, no build step, no utility-class library (Tailwind etc).
  Polish is achieved with better vanilla CSS, not by reaching for a library.
- **Mobile-first.** Default layout is single-column/stacked
  (`flex-direction: column`). Wider layouts are added as
  `@media (min-width: ...)` overrides on top of that, not the other
  way around — see `.home-grid` in `style.css` (stacks by default,
  becomes a 3-column row past `1000px`).
- **Theme via CSS custom properties**, defined once on `:root` and
  overridden under `@media (prefers-color-scheme: light)`. Dark is the
  default/primary palette here — don't hardcode colors in component
  rules, reference the variables (`--bg`, `--card-bg`, `--text`,
  `--muted`, `--accent`, `--border`). Adding new tokens (e.g. a shadow
  color, a gradient stop) for the polish work above is expected —
  define both dark and light values when you do.
- **Consolidate before inventing.** Check `site/static/style.css` for a
  close match first (`.card`, `.button-row`, `.toggle`, picker-button
  patterns, `.flash`) — but when you touch one of these, upgrading its
  visual quality is in scope, not just reusing it as-is.
- **Comment only the non-obvious.** The existing CSS comments explain a
  specific gotcha (why `.topnav` needs an explicit `z-index`, why the
  preview canvas's backing store is 4x its displayed size) — not what a
  rule visually does. Follow that bar: skip comments that just restate
  the CSS.

## The OLED-honesty rule

Any UI previewing device content (currently `#preview-canvas` in the
dashboard, driven by `site/static/card-preview.js`) must represent what
will actually render on the real 128×64 monochrome SSD1306 — same
resolution ratio, same 1-bit rendering, no color, no anti-aliased web
fonts standing in for the device font. It's fine for the canvas's
internal backing store to be a multiple of 128×64 (currently 4x, scaled
down via CSS) for crisper on-screen text, but the *logical* content and
proportions must match the device exactly. If you add a new
previewable field (alignment, corner emoji, etc.), it must show up in
this preview before it ships.

## Palette reference (dark default, `:root` in `style.css`)

Starting point, not a hard limit — extend it (shadow tints, gradient
stops, a second accent for emphasis) as the polish work above calls
for it, defining both dark and light values for anything new.

| Token | Dark | Light |
|---|---|---|
| `--bg` | `#14151a` | `#f5f5f8` |
| `--card-bg` | `#1e1f26` | `#ffffff` |
| `--text` | `#eaeaf0` | `#16171c` |
| `--muted` | `#9a9aa8` | `#66667a` |
| `--accent` | `#6c8cff` | `#3557e0` |
| `--border` | `#2c2d36` | `#e1e1e8` |

Form fields and buttons use a near-black `#0f1015` background in dark
mode regardless of `--bg`/`--card-bg` (see `input[type="text"]`,
`.emoji-btn`) — that's intentional, keep new form controls consistent
with it rather than reusing `--card-bg`.

## Layout patterns already established

- **Centered narrow page** (login, default `main`): `max-width: 640px;
  margin: 0 auto;`.
- **Wide multi-column page** (dashboard): `.wide-page` removes the
  max-width, `.home-grid` stacks on mobile and becomes a row of
  independently-scrolling columns (`overflow-y: auto` per column, not
  the page) above `1000px` — see the `@media (min-width: 1000px)`
  block. Use this pattern for any other page that needs desktop
  side-by-side panels.
- **Picker grids** (`.card-picker`, `.emoji-row`): `display: flex;
  flex-wrap: wrap; gap: ...` of small fixed-size square/pill buttons,
  with a `.active` modifier class swapping to the accent color.
- **Toggle switches**, not native checkboxes, for any on/off setting —
  copy the `.toggle`/`.toggle-track`/`.toggle-thumb` markup+CSS pattern
  rather than styling a bare `<input type="checkbox">`.
- **Flash confirmations**: every state-changing action should flash a
  message (`flash(..., "success")` / `"error"` in `app.py`), rendered
  by the `.flash-stack` block already in `base.html` — don't add a
  separate ad-hoc "saved!" UI.
