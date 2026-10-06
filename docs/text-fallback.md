# Font fallback, emoji and styled text

## Fallback chains

```cpp
fm.load_font("ui", "Roboto-Regular.ttf");
for (const auto &f : system_fonts::cjk())       // .ttc faces load by index
  { fm.load_font("cjk", load_font_face(f.path.c_str(), f.face_index)); break; }
fm.set_fallbacks("ui", {"cjk"});
```

A glyph `ui` lacks is measured and drawn from the first face in its chain
that has it. The lookup happens inside the backend's `measure_text` and
`draw_text_ex`, so wrap, ellipsis, layout, selection and drawing all see the
same glyph without knowing about fallbacks. `set_fallbacks` replaces the
chain and drops the measurement caches; reloading a face re-applies it.
Backends expose the same thing on raw handles: `set_font_fallbacks(Font,
std::vector<Font>)` and `font_fallbacks(Font)`.

- **sokol:** fontstash's own per-glyph fallback (`fonsAddFallbackFont`), used
  by both `fonsTextAdvance` and `fonsDrawText`. `fonsResetFallbackFont` was
  added from upstream fontstash so a chain can be replaced. Fallback glyphs
  sit on the primary's baseline.
- **raylib:** has no fallback, so `place_glyphs` implements it. Once a string
  needs a fallback face or a replacement, `measure_text` and `draw_text_ex`
  both take its per-codepoint layout (DrawTextEx's advance and spacing,
  fallback glyphs moved onto the primary's baseline, rotation via the same
  transform as DrawTextPro). Strings the primary covers keep raylib's own
  `MeasureTextEx`/`DrawTextEx`. raylib bakes glyphs at load, so a fallback
  face covers only the codepoints it was loaded with: pass them to
  `load_font_face(path, index, size, codepoints)`. A CJK face loaded with
  the default set covers no ideographs. Multi-line strings on this path step
  down by `size + 2`, raylib's default line spacing; a changed
  `SetTextLineSpacing` is not seen. No kerning, as in raylib.
- **none:** `set_font_fallbacks` returns false.

Kerning across a primary/fallback join is looked up in the primary's table on
sokol (fontstash behaviour). Measurement and drawing do the same thing, so
they agree; the pair rarely exists.

## Coverage queries

`font_has_glyph(Font, cp)` answers per face from the cmap recorded at load,
on both backends. `FontManager::has_glyph(name, cp)` is per face;
`face_for(name, cp)` names the face that will draw it (empty: none will),
and `missing_codepoints(name, text)` lists what neither the face nor its
chain covers. `font_coverage::extract_face(bytes, index)` turns one face of a
collection into a standalone font; `load_font_face` uses it on both backends.

## System faces

`system_fonts::cjk()`, `emoji()` and `symbols()` return the faces that exist,
in preference order, as `{path, face_index}`.

| | macOS (verified) | Linux / Windows (paths only, untested) |
|---|---|---|
| CJK | Hiragino Sans W3, Hiragino Sans GB, PingFang (pre-12), Apple SD Gothic Neo (Hangul), Arial Unicode | Noto Sans CJK `.ttc` (face 0 = JP), WenQuanYi; Yu Gothic, YaHei, Malgun, SimSun |
| emoji with outlines | none ships | Noto Emoji (monochrome); Segoe UI Emoji |
| symbols, U+FFFD | Arial Unicode, Apple Symbols | DejaVu Sans; Segoe UI Symbol |

Hiragino has no Hangul; chain Apple SD Gothic Neo after it for Korean.

## Emoji: what renders

stb_truetype draws outlines only. Apple Color Emoji is sbix bitmaps and Noto
Color Emoji is CBDT, so neither draws anything. Codepoints no face in the
chain covers go through `font_coverage::visible_text` before measuring and
drawing, on both backends:

- each uncovered codepoint becomes one replacement glyph: **U+FFFD** if a
  face in the chain has it (Arial Unicode does), otherwise **`?`**;
- a ZWJ sequence (👨‍👩‍👧) and a flag pair (🇯🇵) become one replacement;
- uncovered variation selectors, ZWJ, skin-tone modifiers, tags and the
  keycap mark vanish, so `❤️` draws as `❤` when a face has U+2764.

On macOS today an emoji therefore draws as one `?` or `�`, measured exactly
as drawn. BMP symbols (❤ ★ ☺) draw in one colour when a chained face covers
them. A colour emoji path (CoreText rasterising into an RGBA atlas next to
fontstash's alpha atlas) is not implemented.

## Styled text

`TextSpan{text, color, weight, font, size_scale}`: `font` names a registered
face (empty: the label's) and `size_scale` multiplies the label's font size,
so a run keeps its proportion at every UI scale.

`layout_styled_text(runs, metrics, wrap_width, ellipsis_width, alignment,
align_width)` (`plugins/ui/styled_text.h`) returns lines and placed runs.
Both label renderers draw exactly those runs and `AutoLayout` sizes
`Dim::Text` from the same layout. `StyledFonts` supplies metrics from a
`FontManager`; tests may pass their own.

One geometry, `detail::line_x_at`, serves wrapping, ellipsis, run offsets
and selection. A line is split into stretches of equal face, weight and
size; each is measured whole. The glyph starting at byte k is drawn at
`measure(stretch[0, k+len)) - measure(glyph k)`, which is the backend's pen
when it draws the stretch in one call (advances before k plus the spacing
and kerning into k). Drawing runs one by one at those positions puts every
glyph where a single call would, and a selection edge lands on the glyph's
drawn origin. Runs share the lowest baseline any of them needs
(`font_baseline`), and a line is as tall as its tallest run.

- **Ellipsis** (`detail::ellipsize_line`) keeps the longest codepoint prefix
  that fits with `...` in the style of the run it follows. Styled labels now
  truncate. Plain labels use the same cut, so they no longer split a UTF-8
  sequence.
- **Wrapping** breaks at spaces and between ideographs or kana; closing
  punctuation (、。」）！？ etc.) and ー stay with what precedes them. This is
  the basic kinsoku rule, not UAX #14. Hangul wraps at spaces.
- **Selection:** `text_selection::selection_rects` / `byte_offset_at` accept
  the same run-aware measure; `styled_selection_rects(layout, range, origin,
  metrics)` also uses each line's own top and height.

Measure callables may take `(text, TextSpan)`, `(text, FontWeight)` or
`(text)` (`detail::measure_run`); existing callers keep compiling.

## Tests

```sh
nice -n 19 make -C tests -B text_fallback_test        # sokol, CPU fontstash, macOS
nice -n 19 ./output/text_fallback_test
nice -n 19 make -C tests styled_text_test text_fallback_raylib_test
nice -n 19 ./output/styled_text_test
nice -n 19 ./output/text_fallback_raylib_test          # headless GL, macOS
```

`-B` because the dependency file for a two-source sokol test does not track
the afterhours headers.

- `text_fallback_test`: glyphs are captured from fontstash's draw callback
  on a CPU-only context, through `draw_text_ex`. Covers coverage queries,
  chain order and replacement, `.ttc` faces, measured == drawn advance for
  mixed Latin/CJK at sizes 16, 22.4 and 32, replacement glyphs, and, for a
  styled line with a fallback face, a second face, a 1.5x run and an emoji
  at UI scale 1.0, 1.4 and 2.0: runs drawn singly match one call per
  stretch quad for quad, one shared baseline, every selection edge equals
  its glyph's drawn origin, wrap lines fit and are maximal, and ellipsis
  fits, is maximal and draws as wide as measured.
- `styled_text_test`: both renderers draw the layout's runs, styled
  ellipsis keeps colours, scaled runs share a baseline, and `Dim::Text`
  measures the styled layout.
- `text_fallback_raylib_test`: chain order, the unchanged native path,
  measured == placed extent, replacement, and pixels: a mixed string drawn
  whole equals the same string drawn in two pieces at the measured pen.
