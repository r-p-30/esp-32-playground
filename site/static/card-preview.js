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

// Sparkle's glyph here is only used for cursor-advance/width measurement
// (ctx.measureText) - actual drawing is a real vector shape
// (drawSparkleShape() below), not this character. CP437's closest sparkle
// option is a sun-with-rays, which reads as a sun, not a sparkle - mirrors
// drawSparkleGlyph() in DisplayUI.cpp. Snowflake isn't in this map at all
// for drawing purposes - it's a fixed-size bitmap (SNOWFLAKE_BMP below),
// not a font glyph, same as drawSnowflakeGlyph() in DisplayUI.cpp.
const SHORTCODE_GLYPH = {
  ':heart:': '♥',
  ':smile:': '☺',
  ':smileb:': '☻',
  ':sparkle:': '☼',
  ':diamond:': '♦',
  ':note:': '♪',
  ':notes:': '♫',
};
const GLYPH_FAMILY = {
  '♥': 'heart',
  '☺': 'smile', '☻': 'smile',
  '☼': 'sparkle',
  '♦': 'diamond',
  '♪': 'notes', '♫': 'notes',
};
const EMOJI_CHARS = new Set(Object.values(SHORTCODE_GLYPH));

// Exact same bitmap as SNOWFLAKE_BMP in DisplayUI.cpp (reused from the
// Adafruit_SSD1306 example sketch's logo_bmp) - the actual Adafruit logo
// shape, not a geometric snowflake, but this is the specific image asked
// for, kept pixel-identical to what the device draws.
const SNOWFLAKE_ROWS = [
  '........##......',
  '.......###......',
  '.......###......',
  '......#####.....',
  '####..#####.....',
  '#######.#####...',
  '.######.########',
  '..##..###..#####',
  '...###########..',
  '....##.#.###....',
  '...##.###.#.....',
  '..#########.....',
  '..##########....',
  '.#####..####....',
  '.###.....###....',
  '..........##....',
];
const SNOWFLAKE_W = 16, SNOWFLAKE_H = 16;

function familyAnimateEnabled(family) {
  const el = document.querySelector(`input[name="animate_${family}"]`);
  // No checkbox rendered means this family isn't currently used anywhere
  // on the card (see used_emoji_families() in app.py) - animation would
  // never fire on the device either way, so treat as enabled/moot.
  return el ? el.checked : true;
}

function borderFlashEnabled() {
  const el = document.querySelector('input[name="animate_border"]');
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

// Splits a line into regular characters and ":snowflake:" tokens - that
// shortcode isn't in SHORTCODE_GLYPH (it's a bitmap, not a font glyph -
// see SNOWFLAKE_ROWS below), so it survives the shortcode->glyph
// substitution in renderPreview() as a literal substring that needs its
// own multi-character handling here.
function tokenizeLine(line) {
  const tokens = [];
  let i = 0;
  while (i < line.length) {
    if (line.startsWith(':snowflake:', i)) {
      tokens.push({ snowflake: true });
      i += ':snowflake:'.length;
    } else {
      tokens.push({ ch: line[i] });
      i++;
    }
  }
  return tokens;
}

function lineWidth(ctx, line) {
  let w = 0;
  for (const token of tokenizeLine(line)) {
    if (token.snowflake) {
      w += SNOWFLAKE_W;
    } else {
      ctx.font = charFont(token.ch);
      w += ctx.measureText(token.ch).width;
    }
  }
  return w;
}

// Sparkle: a real 4-point cross (mirrors drawSparkleGlyph() in
// DisplayUI.cpp, which reuses the same shape already drawn on the
// good-night card) instead of a CP437 glyph - the closest CP437
// character is a sun-with-rays, which reads as a sun, not a sparkle.
// x/y is the same baseline-anchor position fillText would have used.
function drawSparkleShape(ctx, x, y, fontPx, animating, progress) {
  const cx = x + fontPx * 0.3, cy = y - fontPx * 0.35;
  let r = fontPx * 0.35;
  if (animating) {
    r *= 0.5 + 0.5 * Math.abs(Math.sin(progress * Math.PI * 4));
  }
  ctx.beginPath();
  ctx.moveTo(cx - r, cy); ctx.lineTo(cx + r, cy);
  ctx.moveTo(cx, cy - r); ctx.lineTo(cx, cy + r);
  ctx.strokeStyle = '#fff';
  ctx.lineWidth = 1;
  ctx.stroke();
}

// Draws the exact SNOWFLAKE_ROWS bitmap, pixel for pixel, at its native
// 16x16 size (drawBitmap() on the device doesn't scale, so neither does
// this) - x/y is the top-left corner, same convention as the device's
// Adafruit_GFX cursor. No true rotation is possible for a fixed bitmap,
// so "spin" orbits its position slightly instead (mirrors
// drawSnowflakeGlyph() in DisplayUI.cpp).
function drawSnowflakeShape(ctx, x, y, animating, progress) {
  let ox = 0, oy = 0;
  if (animating) {
    const p = (progress * 3) % 1;
    const step = Math.floor(p * 4) % 4;
    ox = [0, 1, 0, -1][step];
    oy = [-1, 0, 1, 0][step];
  }
  for (let row = 0; row < SNOWFLAKE_H; row++) {
    for (let col = 0; col < SNOWFLAKE_W; col++) {
      if (SNOWFLAKE_ROWS[row][col] === '#') {
        ctx.fillRect(x + ox + col, y + oy + row, 1, 1);
      }
    }
  }
}

// Renders one glyph's default animation, mirroring drawAnimatedGlyph() in
// DisplayUI.cpp - heart/smile/diamond/notes only; sparkle/snowflake are
// vector shapes handled by their dedicated draw*Shape() functions above.
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
    default:
      return false;
  }
}

// Dispatches one glyph's rendering: vector shape (sparkle), animated
// glyph-swap, or a plain character - same precedence as
// printAligned()/drawCornerEmoji() in DisplayUI.cpp. Snowflake isn't
// handled here - it's a multi-character token, not a single glyph, see
// tokenizeLine() and the dedicated calls to drawSnowflakeShape() below.
function drawGlyph(ctx, ch, x, y, fontPx, animating, progress) {
  const family = GLYPH_FAMILY[ch];
  const animateThis = animating && family && familyAnimateEnabled(family);
  if (family === 'sparkle') {
    drawSparkleShape(ctx, x, y, fontPx, animateThis, progress);
    return;
  }
  if (!(animateThis && drawAnimatedGlyph(ctx, family, x, y, fontPx, progress))) {
    ctx.fillText(ch, x, y);
  }
}

function drawLine(ctx, line, x, baselineY, animating, progress) {
  let cx = x;
  for (const token of tokenizeLine(line)) {
    if (token.snowflake) {
      const animateThis = animating && familyAnimateEnabled('snowflake');
      // Bitmap's natural anchor is top-left; baselineY is where regular
      // text sits, so nudge up to roughly center the 16px-tall bitmap
      // on the line instead of it hanging below.
      drawSnowflakeShape(ctx, cx, baselineY - SNOWFLAKE_H + 3, animateThis, progress);
      cx += SNOWFLAKE_W;
      continue;
    }
    const ch = token.ch;
    ctx.font = charFont(ch);
    // Bigger emoji glyphs get nudged down a touch so they read as
    // sitting on the same line instead of hanging off the top of it.
    const y = EMOJI_CHARS.has(ch) ? baselineY + 3 : baselineY;
    drawGlyph(ctx, ch, cx, y, EMOJI_PX, animating, progress);
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
    if (!sel || !sel.value) continue;

    if (sel.value === ':snowflake:') {
      // Fixed 16x16 bitmap, not a font glyph - own symmetric-inset
      // position rather than cornerPosition()'s measureText-based one.
      const isRight = i === 1 || i === 3;
      const isBottom = i === 2 || i === 3;
      const cx = isRight ? (128 - CORNER_INSET - SNOWFLAKE_W) : CORNER_INSET;
      const cy = isBottom ? (64 - CORNER_INSET - SNOWFLAKE_H) : CORNER_INSET;
      const animateThis = animating && familyAnimateEnabled('snowflake');
      drawSnowflakeShape(ctx, cx, cy, animateThis, progress);
      continue;
    }

    const glyph = SHORTCODE_GLYPH[sel.value];
    if (!glyph) continue;
    const [cx, cy] = cornerPosition(ctx, glyph, i, CORNER_PX);
    ctx.font = `${CORNER_PX}px ${FONT_STACK}`;
    drawGlyph(ctx, glyph, cx, cy, CORNER_PX, animating, progress);
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

  if (animating && borderFlashEnabled()) {
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
