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

## Full requirements spec

## TEXT-INPUT COMPONENT — FULL REQUIREMENTS SPEC (what a real text field must do)

Gabe asked for a complete spec of everything we need from the input UI component,
researched against the HTML `<input>`/`<textarea>` editing model, Dear ImGui's
`InputText` (backed by `stb_textedit`), egui's `TextEdit`, and native macOS text
behavior. Gaps #29/#31/#32/#33/#34/#35 above are individual instances; THIS is the
consolidated target. Grouped by capability, each marked:
  [OK] works in afterhours today · [PARTIAL] half-there · [GAP] missing.

References: WHATWG/HTML editing ("Move the caret" / "Change the selection"),
Dear ImGui `imgui_widgets.cpp` STB_TEXTEDIT_K_* bindings, egui `TextEdit`
(singleline/multiline, desired_rows, clip, cursor/selection, clipboard), macOS
NSText standard key bindings.

### 1. Text entry
- [OK]   Insert printable Unicode (UTF-8) at the caret.
- [OK]   Replace the active selection when typing.
- [GAP]  Reject/round-trip control codes: 0x7F (DEL) and other C0 controls must
         NOT be inserted (gap #31 — macOS backspace emits 0x7F as a CHAR). insert_char
         rejects < 32 but NOT 0x7F.
- [GAP]  IME / dead-key / composed input (accents, CJK) — no composition support.
- [OK]   max_length clamp.

### 2. Deletion
- [OK]   Backspace: delete char before caret (or the selection).
- [OK]   Delete: delete char after caret (or the selection).
- [PARTIAL] Word delete: Ctrl/Alt+Backspace = delete word left; Ctrl/Alt+Delete =
         delete word right. (text_area has word ops; verify single-line text_input.)
- [GAP]  Cmd+Backspace (macOS) = delete to start of line.

### 3. Caret movement
- [OK]   Left / Right by one char (WidgetLeft/WidgetRight) — but was DEAD until the
         app registered afterhours::input::InputSystem (see hanabi build_systems).
- [OK]   Home / End (TextHome / TextEnd) to line start/end.
- [PARTIAL] Word left/right (Ctrl/Alt+Arrow) — move_cursor_word_left/right exist;
         confirm they're MAPPED in the consuming app.
- [GAP]  Up / Down between lines (multiline only — needs the multiline path).
- [GAP]  Cmd+Left/Right (macOS) = line start/end; Cmd+Up/Down = doc start/end.
- [GAP]  Caret must render in the GAP AFTER the last glyph, not inside it (gap #32,
         FIXED on the hanabi/text-input-fixes branch: origin = pad_left).

### 4. Selection
- [OK]   Shift+Left/Right extends selection by char.
- [PARTIAL] Shift+Home/End, Shift+Ctrl/Alt+Arrow (word), Shift+Cmd+Arrow — partial.
- [GAP]  Select-all: Ctrl/Cmd+A. (TextSelectAll action exists; confirm Cmd on macOS.)
- [GAP]  Double-click = select word; triple-click = select line/all.
- [GAP]  Click-drag to select a range; Shift+Click to extend.
- [OK]   Typing / Backspace / Delete replaces the selection.
- [PARTIAL] Selection highlight render (aligned to pad_left now, gap #32 family).

### 5. Clipboard
- [PARTIAL] Cut / Copy / Paste (Ctrl/Cmd+X/C/V). Paste path exists (filters cp<32);
         verify Cmd on macOS + cut/copy of the selection. Paste must also strip
         control chars + (for single-line) collapse/‑strip newlines.
- [GAP]  Paste into multiline should keep newlines (multiline path).

### 6. Undo / redo
- [PARTIAL] push_undo_snapshot() exists (undo stack). Wire Ctrl/Cmd+Z (undo) and
         Ctrl+Shift+Z / Cmd+Shift+Z (redo) bindings + confirm coalescing of runs.

### 7. Submit / newline
- [OK]   Enter fires on_submit (single-line) — but ONLY once the app attaches a
         HasTextInputListener AND registers InputSystem (both were missing in hanabi).
- [GAP]  Shift+Enter = insert newline (gap #33) — needs the multiline render + a
         newline-insert that bypasses the cp<32 guard.
- [GAP]  Escape = clear / revert (gap #35). App-side workaround in hanabi; a
         first-class on_cancel/clear behavior would belong here.

### 8. Layout / rendering
- [PARTIAL] Single-line horizontal scroll (scroll_offset_x) keeping the caret
         visible — exists but text can still paint OUTSIDE the field box in some
         host setups (gap #34): the draw must be SCISSOR-CLIPPED to the content rect.
- [GAP]  Multi-line WRAP mode (wrap at width, grow vertically to a max, then scroll)
         — egui `multiline()` / `desired_rows` / HTML `<textarea>`. Needed for the
         chat composer (gap #33/#34). text_area exists but isn't wrap+submit-wired.
- [PARTIAL] Placeholder text when empty (gap #29 — no native placeholder; hosts
         overlay their own). Should be first-class (HTML `placeholder`).
- [OK]   Caret blink; focus ring.
- [GAP]  Config honored: with_font_size / with_custom_background were ignored
         (gap #17); font derived from height. A field should honor explicit styling.
- [GAP]  Disabled + read-only visual states (read-only: selectable, not editable).

### 9. Focus / interaction
- [OK]   Click to focus + position caret.
- [OK]   Tab / Shift+Tab focus traversal (skip_tabbing supported).
- [GAP]  Focus must not be stolen/reset each frame by the immediate-mode rebuild
         (verify caret/selection persist across frames while focused).
- [GAP]  Mouse: drag-select, double/triple-click (see §4).

### 10. Accessibility / misc
- [GAP]  Screen-reader / a11y role (out of scope for the sprite backend, note it).
- [PARTIAL] Scroll the field into view on focus (host concern).
- [GAP]  Input filtering / validation hook (numeric-only, max, pattern) — ImGui has
         InputTextFlags (CharsDecimal, CharsNoBlank, callbacks). A filter callback
         (accept/transform a codepoint) would cover gap #31 generically.

### PRIORITY for the chat composer (hanabi's actual need), in order:
  1. [GAP] control-char filter (#31) — DONE app-side; belongs in the widget/backend.
  2. [GAP] scissor-clip single-line so text can't escape the box (#34a).
  3. [GAP] multiline wrap + Shift+Enter newline (#33/#34b) — the big one.
  4. [FIXED] caret origin (#32) — landed on hanabi/text-input-fixes.
  5. [GAP] macOS Cmd-based bindings (word/line nav, select-all, clipboard, undo).
  6. [GAP] double/triple-click + drag selection.
  7. [GAP] first-class placeholder (#29) + honor style config (#17).
