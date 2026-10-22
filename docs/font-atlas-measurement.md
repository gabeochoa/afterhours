# Atlas-independent measurement

Sokol `measure_text` uses `fonsTextAdvance`: cached advances or font metrics, without
packing glyph images. It preserves fallback selection, quantization, kerning, rounding,
line height and DPI conversion. No public API or cache was added.

Previously an exhausted atlas skipped uncached advances and stored plausible incomplete
widths in both caches. Rejecting zero widths could not detect that failure.
`font_atlas_measure_test` failed six checks before the fix and now passes, including
exhaustion, both caches, manual expansion, fallback fonts, sizes, spacing and DPI.
Memo 13/13, wrap 58/58, Metal include-order 1/1 and C99 syntax checks also passed.
These tests cover stb_truetype, not optional FreeType or new shaping behavior.

## Cost, September 14, 2026

Apple clang 21, C++20 -O2, shared Apple Silicon laptop, niceness 10. Seven interleaved
before/after runs, no overlapping build; medians of per-run call averages:

| Case | Before | After |
|---|---:|---:|
| Cold glyphs, memo miss | 89.836us | 3.308us |
| New strings, resident glyphs | 2.530us | 2.563us |
| Memo hit | 61.6ns | 68.0ns |

Cold runs reset atlas/memo outside 200 timed calls; resident runs use 2,000 distinct
strings; hits repeat one string 100,000 times. Width checksums match. This is not a
frame benchmark: the first draw still rasterizes. Baseline was `c5cfd35`; raw results
remain in WM's `output/font-atlas-benchmark.json`.

```sh
nice -n 10 make -C tests -j2 font_atlas_measure_test measure_memo_test text_wrap_test
nice -n 10 ./output/font_atlas_measure_test
nice -n 10 make -C tests -B -j2 font_atlas_measure_test CXXFLAGS='-std=c++20 -Wall -Wextra -O2'
nice -n 10 ./output/font_atlas_measure_test --benchmark
```

Fallback drawing and automatic recovery are implemented for the sokol backend:
every loaded face falls back to every other loaded face in load order (see
`mesh_font_fallbacks` in `src/backends/sokol/font_helper.h`), so a glyph missing
from one face draws from the first loaded face that has it; and the
`FONS_ATLAS_FULL` error callback expands the atlas (doubling, cap 4096) so
fontstash's retry draws the glyph instead of omitting it. A codepoint no loaded
face covers still draws nothing (fontstash caches the face's .notdef);
`FontManager::has_glyph` / `missing_codepoints` report those. Hanabi's guard
(`src/util/atlas_guard.h`) is removable once Hanabi bumps to this version. Not
implemented: the raylib backend, whose whole-string `DrawTextEx` path needs
per-codepoint run splitting for fallback, and which has no dynamic atlas to
recover. Direct Fontstash bounds/drawing APIs and the Sokol spacing argument
retain their existing behavior.
