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
./headless_validation --scene=text

# Custom output directory
./headless_validation --output-dir=/tmp/validation

# Allow minor per-channel differences (e.g. anti-aliasing)
./headless_validation --tolerance=2

# Suppress raylib INFO logs
./headless_validation -q

# List available scenes
./headless_validation --list

# Verbose mode (show pixel counts)
./headless_validation -v
```

---

## Output Format

Success (with -q):
```
=== Headless Rendering Validation ===
Resolution:  800x600
Tolerance:   0 (per-channel)
Output dir:  headless_validation_output
==============================
  PASS  filled_rects
  PASS  text
  ...
==============================
30/30 passed
```

Failure:
```
  FAIL  alpha_blending                     42 px (0.01%) differ, max=3, first=(320,240)
```

---

## Artifacts

For each scene, up to 3 files are saved:
- `scene_windowed.png` -- reference capture
- `scene_headless.png` -- headless capture
- `scene_diff.png` -- heat-map diff image (only on failure; brighter red = larger diff)

---

## Implementation Notes

### Backend Re-initialization

Re-initializing the graphics backend multiple times in one process caused state corruption (especially after windowed GLFW shutdown). **Solution**: render all headless scenes in one `init/shutdown` session, then all windowed scenes in another session, then compare offline.

### Font Loading in Headless Mode

Raylib's `LoadFontDefault()` guards texture creation behind a global `isGpuReady` flag that only `InitWindow` sets. **Solution**: the headless backend now sets `isGpuReady = true` after rlgl init and calls `LoadFontDefault()` so that `DrawText`/`GetFontDefault()` work without a window.

### MSAA Parity

The windowed backend hardcodes `FLAG_MSAA_4X_HINT`. Headless has no MSAA. **Solution**: added `Config::enable_msaa` flag; the validation tool disables MSAA for the windowed pass.

### Static Linking

On macOS, the validation tool must link against `libraylib.a` (static) rather than `libraylib.dylib` because `LoadFontDefault` is a local symbol in the dylib. The Makefile specifies the `.a` path directly plus required frameworks (Cocoa, IOKit, CoreGraphics, CoreVideo).

---

## When to Re-run

Run this tool when changing:
- Headless GL context setup
- Drawing helpers
- Render texture handling
- Backend initialization code
- Any rendering pipeline changes

Not part of CI -- a manual validation step.
