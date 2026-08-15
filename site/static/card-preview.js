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
const EMOJI_PX = 18; // inline emoji drawn noticeably larger than surrounding text

const SHORTCODE_GLYPH = {
  ':heart:': '♥',
  ':smile:': '☺',
  ':smileb:': '☻',
  ':spade:': '♠',
  ':club:': '♣',
  ':diamond:': '♦',
  ':note:': '♪',
  ':notes:': '♫',
};
const EMOJI_CHARS = new Set(Object.values(SHORTCODE_GLYPH));

// Mirrors the corner insets in DisplayUI.cpp's drawCornerEmoji().
const CORNER_XY = [
  [3, 11],    // top-left
  [117, 11],  // top-right
  [3, 60],    // bottom-left
  [117, 60],  // bottom-right
];
const CORNER_PX = 11; // "slightly increased" from the base 8px text size

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

function drawLine(ctx, line, x, baselineY) {
  let cx = x;
  for (const ch of line) {
    ctx.font = charFont(ch);
    // Bigger emoji glyphs get nudged down a touch so they read as
    // sitting on the same line instead of hanging off the top of it.
    const y = EMOJI_CHARS.has(ch) ? baselineY + 3 : baselineY;
    ctx.fillText(ch, cx, y);
    cx += ctx.measureText(ch).width;
  }
}

// animProgress: omit for the static view, or 0-1 to also draw the bounce
// ball at that point in its cycle (see previewAnimation() below).
function renderPreview(animProgress) {
  const canvas = document.getElementById('preview-canvas');
  if (!canvas) return;
  const ctx = canvas.getContext('2d');
  const scale = canvas.width / 128;

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
    if (glyph) {
      ctx.font = `${CORNER_PX}px ${FONT_STACK}`;
      ctx.fillText(glyph, CORNER_XY[i][0], CORNER_XY[i][1]);
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
    drawLine(ctx, line, x, y);
    y += lineHeight;
  }

  if (typeof animProgress === 'number') {
    const topY = 20, bottomY = 64 - 12;
    const wave = 1 - Math.abs(2 * animProgress - 1);
    const bounceY = topY + wave * (bottomY - topY);
    ctx.beginPath();
    ctx.arc(6, bounceY, 3, 0, Math.PI * 2);
    ctx.fill();
  }
}

// Simulates drawBounce()'s press-animation (a ball bouncing top-to-bottom
// over the card's configured duration) - only an approximation of that
// one built-in layout, same caveat as everything else in this file.
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
