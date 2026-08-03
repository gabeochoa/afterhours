# Text-input fixes (branch: hanabi/text-input-fixes)

Downstream driver: the **hanabi** app (native macOS Navi client) hit a cluster of
text-input issues. This branch carries fixes + a reference list for the maintainer.

## Landed on this branch
- **Caret/selection origin (gap #32)** — the cursor overlay + selection highlight
  computed x from `-DRAW_TEXT_MARGIN` but the text renders at `pad_left`, so the
  caret drew INSIDE the last glyph. Fixed to `pad_left`. (commit: caret origin)

## Still open (need the multi-line path — larger change, documented for reference)
- **Shift+Enter newline (gap #33):** single-line `text_input` treats Enter as the
  only submit and has no shift+enter → insert '\n'. Needs either (a) a multi-line
  mode on `text_input` (render + measure across '\n', and gate submit vs newline on
  the shift modifier), or (b) the composer swaps to `text_area` (which supports
  newlines) + a submit binding. `insert_char` also rejects '\n' (codepoint < 32) —
  a newline insert path must bypass that guard.
- **Wrap / overflow-clip (gap #34):** long text draws OUTSIDE the field box instead
  of wrapping or staying clipped. There is an `Overflow::Hidden` inner container +
  `scroll_offset_x`, but the text still paints past the field edge in the hanabi
  composer setup. Needs: (a) scissor-clip the text draw to the field content rect so
  h-scroll hides overflow, and/or (b) an opt-in multi-line WRAP mode (pairs with
  shift+enter) so the field grows vertically.
- **Control-char CHAR events (gap #31, macOS):** the sokol macOS backend emits a
  SAPP_EVENTTYPE_CHAR for BACKSPACE (U+007F) and the queue/`insert_char` accept it
  (only `< 32` is rejected), so 0x7F gets typed as a DEL glyph. Minimal fix: in the
  sokol backend's SAPP_EVENTTYPE_CHAR case, `if (c >= 32 && c != 0x7F) push_char(c)`,
  or tighten `insert_char` to also reject 0x7F. (hanabi works around it app-side by
  filtering the char queue each frame.)

See the hanabi repo's afterhours_gaps.md for the full write-ups (#31–#35).
