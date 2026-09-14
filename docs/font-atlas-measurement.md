# Atlas-independent measurement

The Sokol backend now measures advance width without placing glyph images in the
atlas. `fonsTextAdvance` reads an existing glyph's advance when available and
otherwise obtains it from the font. It preserves the previous size quantization,
fallback selection, kerning and integer rounding. The backend's line-height and
DPI conversion stay unchanged. The application-facing API and both text caches
stay unchanged; no new cache is allocated.

Previously, `fonsTextBounds` tried to allocate a glyph image while measuring. When
the atlas was full, it skipped uncached glyphs and their advances. A positive,
plausible but incomplete width then entered both the backend memo and UI cache.
It could remain wrong even after expanding the atlas. Merely rejecting zero widths
would not catch this case.

The existing warning now describes missing glyph images without claiming that
`measure_text` becomes invalid. Its old comment was updated because that claim is
no longer true. Direct Fontstash bounds/drawing APIs retain their existing behavior.

## Regression and compatibility

`font_atlas_measure_test` failed six checks before the production fix. It now passes:

- Fill a 32px atlas with cached and uncached glyphs, then compare measurement with
  a sufficiently large atlas.
- Verify measurement does not add glyph records, modify atlas pixels or trigger
  another atlas-full callback.
- Exercise backend memo hits and the UI text cache before and after manual atlas
  expansion. After expansion, normal drawing's returned advance agrees again.
- Compare against the existing Fontstash bounds path across four sizes, 1x/2x DPI,
  three spacing values and ASCII, UTF-8, missing-glyph and newline inputs.
- Check a real fallback-font glyph, explicit string endpoints and empty inputs.

The tests use the default stb_truetype configuration shipped with Sokol. They do
not certify the optional FreeType configuration or add shaping/line-breaking
behavior. Existing fallback kerning semantics are preserved, not redesigned.
The afterhours Sokol spacing argument remains unchanged by this fix.

The adjacent measure-memo checks pass 13/13 and text-wrap checks 58/58. The Metal
implementation compiles and its include-order check passes 1/1. The altered
Fontstash implementation also passes a C99 syntax check. No full UI/screenshot
suite was needed for this Sokol-specific measurement change.

Run from the afterhours repository:

```sh
nice -n 10 make -C tests -j2 font_atlas_measure_test measure_memo_test text_wrap_test
nice -n 10 ./output/font_atlas_measure_test
nice -n 10 ./output/measure_memo_test
nice -n 10 ./output/text_wrap_test
```

## Cost measured September 14, 2026

Apple clang 21, C++20, `-O2`, reduced priority, seven interleaved before/after runs
on the shared Apple Silicon laptop. No build ran concurrently. The baseline binary
was captured before the production edits at `c5cfd35`, using the same benchmark
loop. Values below are medians of each run's per-call average.

| Case | Before | After |
|---|---:|---:|
| Cold glyphs, memo miss | 89.836 us | 3.308 us |
| New strings, glyphs already resident | 2.530 us | 2.563 us |
| Backend memo hit | 61.6 ns | 68.0 ns |

Cold measurement is about 27 times faster in this fixture because it no longer
rasterizes or packs images. Resident-glyph misses are effectively level. Cache-hit
cost increased by 6.4 ns in this sample, about 0.0064 ms per 1,000 calls; there is
no additional cache lookup on a hit. This microbenchmark does not measure complete
frame time or establish a universal speedup.

The cold case resets the atlas and memo before each of 200 timed calls, excluding
reset cost. The resident case uses 2,000 distinct strings after priming glyphs.
The hit case repeats one cached string 100,000 times. All runs produced the same
width checksum. The first draw after a cold measurement still needs to rasterize
its glyphs; this defers that work to drawing rather than eliminating it.

For an optimized benchmark binary, rebuild explicitly because changing compiler
flags alone does not invalidate the test target:

```sh
nice -n 10 make -C tests -B -j2 font_atlas_measure_test CXXFLAGS='-std=c++20 -Wall -Wextra -O2'
nice -n 10 ./output/font_atlas_measure_test --benchmark
```

Raw measurements from this run are retained locally in WM's
`output/font-atlas-benchmark.json`.

## Still deferred

Rendering can still drop glyph images when the atlas is full. Drawing missing-glyph
boxes, preserving render advances when images are absent, automatic growth/reset
and generalized measurement error reporting remain separate work. Hanabi's guard
is retained. This change closes the atlas-dependent width/cache defect only.
