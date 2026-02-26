# Headless Rendering Validation Tool

## Problem

The afterhours engine supports both headless (offscreen) and windowed rendering modes. Headless mode is significantly faster for testing because it doesn't create an OS window. However, there's no way to verify that headless rendering produces pixel-identical output to windowed rendering. In the past, differences between the modes have caused regressions. Before migrating all tests to headless, we need a way to prove equivalence.

## Goal

Build a standalone validation binary that renders the same scenes in multiple backend modes and performs bit-for-bit pixel comparison. This is a migration validation tool -- prove equivalence once, switch tests to headless, re-run periodically when the rendering pipeline changes.

---

## Design

### Overall Structure

A single file: `examples/headless_validation.cpp`

When run, it:
1. Defines an array of synthetic test scenes (functions) that exercise rendering primitives and layout.
2. For each scene:
   - **Reference pass**: Init with `DisplayMode::Windowed` + `FLAG_WINDOW_HIDDEN` (creates real GL context without showing a window), render the scene, call `capture_frame()` to save a PNG, shut down.
   - **Headless pass**: Init with `DisplayMode::Headless`, render the same scene, call `capture_frame()` to save a PNG, shut down.
   - **Metal pass** (when `AFTER_HOURS_USE_METAL` is defined): Init metal windowed with hidden window, render, capture, shut down.
   - **Compare**: Load both captured PNGs, compare every pixel byte-by-byte. On mismatch, save a diff image and record the failure.
3. Exit with code 0 if all scenes match, non-zero if any differ. Print a summary.

### Multi-Backend Comparison Matrix

The reference mode is raylib windowed (hidden window) since it's the most battle-tested path. Every other mode is compared against it:

- `raylib_headless` vs `raylib_windowed` (reference)
- `metal_windowed` vs `raylib_windowed` (reference), when Metal backend is compiled in

### Pixel Comparison

- Exact bit-for-bit match required (zero tolerance)
- Fast path: `memcmp` on the entire pixel buffer
- On mismatch: pixel-by-pixel walk to find exact coordinates and color values
- Diff image output: white pixels = match, red pixels = mismatch
- Uses raylib's `LoadImage()` for PNG loading (pure file I/O, no window needed)

### Test Scenes

Each scene is a function with the signature:

```cpp
struct Scene {
    const char* name;
    void (*render)(int width, int height);
};
```

Scenes render exactly one frame to a fixed resolution (800x600) for deterministic output. No animation, no timing dependence.

#### Tier 1 -- Primitive Rendering

Tests whether backends draw the same shapes:

- `filled_rectangles` -- Solid colored rects at various positions, sizes, colors
- `rectangle_outlines` -- Outlines with different thicknesses
- `rounded_rectangles` -- Per-corner rounding control
- `circles_lines_triangles` -- Circles, lines, and triangles
- `text_rendering` -- Text at different sizes and positions (default embedded font)
- `alpha_blending` -- Semi-transparent overlapping rects

#### Tier 2 -- Layout + Rendering

Tests whether layout produces identical visuals:

- `flex_column_row` -- Column and row stacking with fixed sizes
- `flex_grow_weighted` -- `expand()` with weighted children
- `justify_content_all` -- All 5 justify modes in both directions
- `align_items_all` -- All align_items modes including stretch
- `wrapping_with_gap` -- Wrapping with gap property
- `min_max_constraints` -- Min/max constraints clamping visible size
- `absolute_positioning` -- Absolute positioned elements overlapping flow children
- `nested_layouts` -- Holy grail, sidebar, card grid composite layouts

#### Tier 3 -- Visual Components

Tests whether decorations render identically:

- `borders` -- Per-side color and thickness
- `bevel_borders` -- Raised/sunken 3D borders
- `shadows` -- Hard and soft shadow styles
- `scissor_clipping` -- Overflow hidden clipping
- `opacity` -- Semi-transparent components

#### Tier 4 -- Edge Cases

- `subpixel_rounding` -- Odd-width containers split among children
- `screen_edges` -- Elements at (0,0) and max bounds
- `zero_and_tiny` -- Zero-size and single-pixel elements
- `deep_nesting` -- Deep nesting with padding accumulation

---

## Program Interface

```bash
# Run all scenes
./headless_validation

# Run a specific scene
./headless_validation --scene=filled_rectangles

# Custom output directory
./headless_validation --output-dir=/tmp/validation
```

---

## Output Format

Success:
```
Headless Rendering Validation
Resolution: 800x600
==============================
filled_rectangles ........... raylib_headless vs raylib_windowed: PASS
rectangle_outlines .......... raylib_headless vs raylib_windowed: PASS
...
==============================
30/30 passed, 0 failed
```

Failure:
```
alpha_blending .............. raylib_headless vs raylib_windowed: FAIL
  First mismatch at (320, 240): #FF0000FF vs #FE0000FF
  Diff image saved: output/alpha_blending_diff.png
```

---

## Artifacts

For each scene, up to 3 files are saved:
- `scene_windowed.png` -- reference capture
- `scene_headless.png` -- headless capture
- `scene_diff.png` -- diff image (only on failure)

---

## Risk: Backend Re-initialization

Reinitializing the graphics backend multiple times in one process (init, shutdown, init again) needs to work cleanly. If that proves fragile, the fallback is running each pass as a separate child process, but the simple approach should be tried first.

---

## When to Re-run

Run this tool when changing:
- Headless GL context setup
- Drawing helpers
- Render texture handling
- Backend initialization code
- Any rendering pipeline changes

Not part of CI -- a manual validation step.
