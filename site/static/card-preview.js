// Client-side approximation of how DisplayUI.cpp's drawBounce() renders a
// card on the real 128x64 OLED - not pixel-perfect (browser font metrics
// don't exactly match the device's font), but close enough to catch
// alignment/overflow/corner-emoji-collision mistakes before saving.

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

// Mirrors the corner insets in DisplayUI.cpp's drawCornerEmoji().
const CORNER_XY = [
  [3, 10],    // top-left
  [119, 10],  // top-right
  [3, 61],    // bottom-left
  [119, 61],  // bottom-right
];

function renderPreview() {
  const canvas = document.getElementById('preview-canvas');
  if (!canvas) return;
  const ctx = canvas.getContext('2d');

  ctx.fillStyle = '#000';
  ctx.fillRect(0, 0, 128, 64);
  ctx.strokeStyle = '#fff';
  ctx.lineWidth = 1;
  ctx.strokeRect(0.5, 0.5, 127, 63);

  ctx.fillStyle = '#fff';
  ctx.font = '8px monospace';
  ctx.textBaseline = 'alphabetic';

  for (let i = 0; i < 4; i++) {
    const sel = document.getElementById('corner' + i);
    const glyph = sel && SHORTCODE_GLYPH[sel.value];
    if (glyph) ctx.fillText(glyph, CORNER_XY[i][0], CORNER_XY[i][1]);
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
    const w = ctx.measureText(line).width;
    let x;
    if (alignH === 'left') {
      x = xLeft;
    } else if (alignH === 'right') {
      x = xRight - w;
    } else {
      x = xLeft + (xRight - xLeft - w) / 2;
    }
    if (x < xLeft) x = xLeft;
    ctx.fillText(line, x, y);
    y += lineHeight;
  }
}
