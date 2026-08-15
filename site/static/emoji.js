// Inserts an emoji shortcode at the cursor position in the given textarea.
// Only the 8 shortcodes the OLED firmware can actually render (see
// docs/remote-api-spec.md) - deliberately not a general emoji picker.
function insertShortcode(textareaId, code) {
  const el = document.getElementById(textareaId);
  const start = el.selectionStart;
  const end = el.selectionEnd;
  el.value = el.value.slice(0, start) + code + el.value.slice(end);
  const cursor = start + code.length;
  el.focus();
  el.setSelectionRange(cursor, cursor);
}
