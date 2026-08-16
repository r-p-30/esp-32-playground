// Client-side approximation of how DisplayUI.cpp's drawBounce() renders a
// card on the real 128x64 OLED - not pixel-perfect (browser font metrics
// don't exactly match the device's font), but close enough to catch
// alignment/overflow/corner-emoji-collision mistakes before saving.
//
// The canvas backing store is rendered at 4x the logical 128x64 OLED
// resolution (see the width/height attributes in dashboard.html) and all
// drawing below still uses plain 0-128/0-64 coordinates via a scale
// transform - this is what keeps the text crisp instead of blurry, since
// it's native resolution rather than the browser stretching a tiny bitmap.

// Falls back through fonts with real glyph coverage for the CP437 symbol
// characters below - the plain "monospace" stack on some systems is
// missing glyphs for U+266A/U+266B (note/notes), which silently renders
// nothing instead of even a placeholder box.
const FONT_STACK = '"Segoe UI Symbol", "Segoe UI Emoji", "DejaVu Sans", monospace';
const NORMAL_PX = 8;
const EMOJI_PX = 14;   // inline emoji drawn larger than surrounding text, not 2x+ (that read as oversized)
const CORNER_PX = 9;   // "slightly increased" from the base 8px text size
const CORNER_INSET = 3;  // px from each edge, logical 128x64 space - same on every side

// Mirrors Cards.cpp's glyphToEmojiAnimFamily() - which family (if any)
// each shortcode's glyph belongs to, for the per-family animate toggles
// and the animations in drawAnimatedGlyph() below.
const SHORTCODE_GLYPH = {
  ':heart:': '♥',
  ':smile:': '☺',
  ':smileb:': '☻',
  ':sparkle:': '☼',
  ':diamond:': '♦',
  ':note:': '♪',
  ':notes:': '♫',
  ':snowflake:': '•',
};
const GLYPH_FAMILY = {
  '♥': 'heart',
  '☺': 'smile', '☻': 'smile',
  '☼': 'sparkle',
  '♦': 'diamond',
  '♪': 'notes', '♫': 'notes',
  '•': 'snowflake',
};
const EMOJI_CHARS = new Set(Object.values(SHORTCODE_GLYPH));

function familyAnimateEnabled(family) {
  const el = document.querySelector(`input[name="animate_${family}"]`);
  // No checkbox rendered means this family isn't currently used anywhere
  // on the card (see used_emoji_families() in app.py) - animation would
  // never fire on the device either way, so treat as enabled/moot.
  return el ? el.checked : true;
}

// Symmetric inset from every edge, converting the canvas's baseline-relative
// y-coordinate into the same "top-left of the glyph" positioning the
// device's Adafruit_GFX cursor uses - without this, a numerically-equal
// inset on the x and y axes still renders visually off-center, since
// fillText's y is where the text *sits*, not its top edge.
function cornerPosition(ctx, glyph, cornerIndex, fontPx) {
  ctx.font = `${fontPx}px ${FONT_STACK}`;
  const m = ctx.measureText(glyph);
  const ascent = m.actualBoundingBoxAscent || fontPx * 0.8;
  const width = m.width;
  const isRight = cornerIndex === 1 || cornerIndex === 3;
  const isBottom = cornerIndex === 2 || cornerIndex === 3;
  const x = isRight ? (128 - CORNER_INSET - width) : CORNER_INSET;
  const y = isBottom ? (64 - CORNER_INSET) : (CORNER_INSET + ascent);
  return [x, y];
}

function charFont(ch) {
  return `${EMOJI_CHARS.has(ch) ? EMOJI_PX : NORMAL_PX}px ${FONT_STACK}`;
}

function lineWidth(ctx, line) {
  let w = 0;
  for (const ch of line) {
    ctx.font = charFont(ch);
    w += ctx.measureText(ch).width;
  }
  return w;
}

// Renders one glyph's default animation, mirroring drawAnimatedGlyph() in
// DisplayUI.cpp - same 5 (well, 6) simple effects, same rough timing.
// Returns true if it drew something custom (caller skips the plain draw).
function drawAnimatedGlyph(ctx, family, x, y, fontPx, progress) {
  const cycle = (hz) => (progress * hz) % 1;
  ctx.font = `${fontPx}px ${FONT_STACK}`;

  switch (family) {
    case 'heart': {
      const p = cycle(3);
      const visible = p < 0.15 || (p > 0.25 && p < 0.4);
      if (visible) ctx.fillText('♥', x, y);
      return true;
    }
    case 'smile': {
      const g = cycle(6) < 0.5 ? '☺' : '☻';
      ctx.fillText(g, x, y);
      return true;
    }
    case 'sparkle': {
      const p = cycle(8);
      if (p < 0.55) {
        ctx.fillText('☼', x, y);
        if (p < 0.15) {
          ctx.fillRect(x - fontPx * 0.3, y - fontPx * 0.1, 1, 1);
          ctx.fillRect(x + fontPx * 0.9, y - fontPx * 0.1, 1, 1);
        }
      }
      return true;
    }
    case 'diamond': {
      ctx.fillText('♦', x, y);
      const p = cycle(4);
      if (p < 0.2) {
        const o = fontPx * 0.5;
        ctx.fillRect(x - o, y - o, 1, 1);
        ctx.fillRect(x + fontPx + o, y - o, 1, 1);
        ctx.fillRect(x - o, y + o, 1, 1);
        ctx.fillRect(x + fontPx + o, y + o, 1, 1);
      }
      return true;
    }
    case 'notes': {
      const p = cycle(5);
      const g = p < 0.5 ? '♪' : '♫';
      const yOff = (p < 0.25 || (p >= 0.5 && p < 0.75)) ? 0 : -fontPx * 0.1;
      ctx.fillText(g, x, y + yOff);
      return true;
    }
    case 'snowflake': {
      const p = cycle(3);
      const step = Math.floor(p * 4) % 4;
      const ox = [0, 1, 0, -1][step] * (fontPx * 0.15);
      const oy = [-1, 0, 1, 0][step] * (fontPx * 0.15);
      ctx.fillText('•', x + ox, y + oy);
      return true;
    }
    default:
      return false;
  }
}

function drawLine(ctx, line, x, baselineY, animating, progress) {
  let cx = x;
  for (const ch of line) {
    ctx.font = charFont(ch);
    // Bigger emoji glyphs get nudged down a touch so they read as
    // sitting on the same line instead of hanging off the top of it.
    const y = EMOJI_CHARS.has(ch) ? baselineY + 3 : baselineY;
    const family = GLYPH_FAMILY[ch];
    const animateThis = animating && family && familyAnimateEnabled(family);
    if (!(animateThis && drawAnimatedGlyph(ctx, family, cx, y, EMOJI_PX, progress))) {
      ctx.fillText(ch, cx, y);
    }
    cx += ctx.measureText(ch).width;
  }
}

// animProgress: omit for the static view, or 0-1 to also animate emoji
// and draw the border-flash ring (see previewAnimation() below).
function renderPreview(animProgress) {
  const canvas = document.getElementById('preview-canvas');
  if (!canvas) return;
  const ctx = canvas.getContext('2d');
  const scale = canvas.width / 128;
  const animating = typeof animProgress === 'number';
  const progress = animating ? animProgress : 0;

  ctx.setTransform(scale, 0, 0, scale, 0, 0);
  ctx.fillStyle = '#000';
  ctx.fillRect(0, 0, 128, 64);
  ctx.strokeStyle = '#fff';
  ctx.lineWidth = 1;
  ctx.strokeRect(0.5, 0.5, 127, 63);

  ctx.fillStyle = '#fff';
  ctx.textBaseline = 'alphabetic';

  for (let i = 0; i < 4; i++) {
    const sel = document.getElementById('corner' + i);
    const glyph = sel && SHORTCODE_GLYPH[sel.value];
    if (!glyph) continue;
    const [cx, cy] = cornerPosition(ctx, glyph, i, CORNER_PX);
    const family = GLYPH_FAMILY[glyph];
    const animateThis = animating && family && familyAnimateEnabled(family);
    ctx.font = `${CORNER_PX}px ${FONT_STACK}`;
    if (!(animateThis && drawAnimatedGlyph(ctx, family, cx, cy, CORNER_PX, progress))) {
      ctx.fillText(glyph, cx, cy);
    }
  }

  const textEl = document.getElementById('text');
  let text = textEl ? textEl.value : '';
  for (const [code, glyph] of Object.entries(SHORTCODE_GLYPH)) {
    text = text.split(code).join(glyph);
  }

  const alignHEl = document.querySelector('input[name="alignH"]:checked');
  const alignVEl = document.querySelector('input[name="alignV"]:checked');
  const alignH = alignHEl ? alignHEl.value : 'center';
  const alignV = alignVEl ? alignVEl.value : 'middle';

  const lines = text.split('\n');
  const lineHeight = 9;
  const blockHeight = lines.length * lineHeight;
  const xLeft = 4, xRight = 128 - 4, yTop = 0, yBottom = 64;

  let y;
  if (alignV === 'top') {
    y = yTop + 8;
  } else if (alignV === 'bottom') {
    y = Math.max(yTop, yBottom - blockHeight) + 8;
  } else {
    y = yTop + Math.max(0, (yBottom - yTop) - blockHeight) / 2 + 8;
  }

  for (const line of lines) {
    const w = lineWidth(ctx, line);
    let x;
    if (alignH === 'left') {
      x = xLeft;
    } else if (alignH === 'right') {
      x = xRight - w;
    } else {
      x = xLeft + (xRight - xLeft - w) / 2;
    }
    if (x < xLeft) x = xLeft;
    drawLine(ctx, line, x, y, animating, progress);
    y += lineHeight;
  }

  if (animating) {
    // Mirrors drawBounce()'s border-flash in DisplayUI.cpp.
    const phase = (progress * 6) % 1;
    if (phase < 0.5) {
      ctx.strokeRect(2.5, 2.5, 123, 59);
    }
  }
}

// Simulates drawBounce()'s press-animation (border flash + per-emoji
// effects) over the card's configured duration - only an approximation,
// same caveat as everything else in this file.
let previewAnimFrame = null;

function previewAnimation() {
  if (previewAnimFrame) return;
  const durationEl = document.getElementById('durationMs');
  const duration = Math.max(100, parseInt(durationEl && durationEl.value, 10) || 1000);
  const start = performance.now();

  function frame(now) {
    const progress = Math.min((now - start) / duration, 1);
    renderPreview(progress);
    if (progress < 1) {
      previewAnimFrame = requestAnimationFrame(frame);
    } else {
      previewAnimFrame = null;
      renderPreview();
    }
  }
  previewAnimFrame = requestAnimationFrame(frame);
}
