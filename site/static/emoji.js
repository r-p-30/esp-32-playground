// Inserts an emoji shortcode at the cursor position in the given textarea.
// Only the 8 shortcodes the OLED firmware can actually render (see
// docs/remote-api-spec.md) - deliberately not a general emoji picker.
function insertShortcode(textareaId, code) {
  const el = document.getElementById(textareaId);
  el.focus();

  // execCommand('insertText', ...) goes through the browser's native
  // text-editing pipeline, same as typing - unlike setting el.value
  // directly, this puts the insertion on the undo stack (Ctrl+Z) and
  // fires a real 'input' event on its own. Setting el.value bypasses
  // both, since the browser has no way to know a programmatic value
  // change was an "edit" rather than e.g. resetting a form.
  const inserted = document.execCommand && document.execCommand('insertText', false, code);
  if (inserted) return;

  // Fallback for the rare browser without execCommand support - no undo
  // entry, but at least the insertion itself still works.
  const start = el.selectionStart;
  const end = el.selectionEnd;
  el.value = el.value.slice(0, start) + code + el.value.slice(end);
  const cursor = start + code.length;
  el.setSelectionRange(cursor, cursor);
  el.dispatchEvent(new Event('input', { bubbles: true }));
}
