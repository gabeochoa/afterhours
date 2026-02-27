# Cross-Backend Rendering Validation

## Goal

Same afterhours UI code must produce **pixel-identical** output regardless of
which graphics backend is active (raylib, sokol/metal, future backends).

---

## Current State

| Backend | Drawing | Text Rendering | Text Measurement | Headless | Screenshot Capture |
|---------|---------|----------------|------------------|----------|--------------------|
| **Raylib** | OpenGL via rlgl | raylib bitmap font / LoadFontEx | `MeasureTextEx` (raylib) | Yes (CGL) | Yes (`LoadImageFromTexture`) |
| **Sokol/Metal** | sokol_gl (Metal) | fontstash + stb_truetype | `fonsTextBounds` (fontstash) | No | Partial (window capture only) |
| **None** | Stubs | Stubs | Stubs | N/A | N/A |

The existing `headless_validation` tool validates **within** raylib only
(headless GL vs windowed GL). It does not test cross-backend equivalence.

---

## Why Pixel-Perfect is Hard

### 1. Text Measurement Divergence

Raylib's `MeasureTextEx` and fontstash's `fonsTextBounds` use different font
rasterizers (raylib internal vs stb_truetype). Even for the same TTF file:

- Glyph advance widths may differ by ±1px at certain sizes
- This shifts **all** layout downstream (flex children, text truncation, etc.)

**Impact**: Layout differences propagate -- a 1px text width difference can
shift every sibling element.

### 2. Font Rasterization Divergence

- Raylib: bitmap font from pixel data (default) or `LoadFontEx` (stb_truetype)
- Sokol: fontstash (stb_truetype), rendered to atlas via `sfons`

Even when both use stb_truetype under the hood, different atlas sizes, padding,
and rendering parameters produce different glyph bitmaps.

### 3. Shape Rasterization

- Raylib: OpenGL immediate mode via rlgl
- Sokol: sokol_gl immediate mode (on top of Metal/GL/D3D)

Filled rectangles should match (axis-aligned quads). Rounded rectangles and
circles may differ due to different tessellation (segment count, angle steps).

### 4. Anti-Aliasing

Raylib windowed defaults to 4x MSAA. Sokol Metal uses swapchain sample count
(often 4x on Retina). Headless has no MSAA. All must match when comparing.

---

## Architecture

### Approach: Separate Binaries, Shared Scenes

Since backends are compile-time (`#ifdef`), we cannot have both in one binary.
Instead:

```
cross_backend_validation_raylib.exe   --output-dir=out/raylib
cross_backend_validation_metal.exe    --output-dir=out/metal
cross_backend_compare                 out/raylib out/metal
```

Or a single orchestrator script:

```bash
./scripts/validate_cross_backend.sh
# 1. Builds both binaries
# 2. Runs each, capturing PNGs
# 3. Runs comparison tool
# 4. Reports results
```

### Scene Definitions

Scenes use **only** the afterhours drawing helpers and UI API -- no raw
backend calls. This is the contract: if you use the afterhours API, the
output must be identical.

```cpp
// cross_backend_scenes.h -- shared between all backend builds
struct CBScene {
    const char* name;
    void (*render)();
};

// Tier 1: Drawing helpers (no UI/ECS)
void cb_scene_filled_rects();
void cb_scene_rounded_rects();
void cb_scene_text_basic();
void cb_scene_text_sizes();
void cb_scene_alpha_blending();
void cb_scene_scissor_clip();

// Tier 2: Afterhours UI (ECS + autolayout + rendering)
void cb_scene_ui_buttons();
void cb_scene_ui_text_input();
void cb_scene_ui_nested_layout();
void cb_scene_ui_theme_colors();
```

### Comparison Levels

Not all differences are equal. The tool should report at three levels:

1. **Exact match** (0 pixel diff) -- ideal
2. **Tolerance match** (within --tolerance=N per channel) -- acceptable for
   anti-aliasing
3. **Layout match** (same bounding boxes, different sub-pixel rendering) --
   acceptable for text rasterization if glyph positions match

---

## Implementation Plan

### Phase 1: Unify Text Measurement

**Goal**: Identical `measure_text()` results across backends.

Both backends already use stb_truetype when loading external fonts. The
divergence comes from:
- Raylib's default bitmap font (used when no font is loaded)
- Different `spacing` calculations

**Fix**: Create `afterhours::text::measure()` that uses stb_truetype directly,
bypassing backend-specific measurement. The UI library calls this unified
function instead of the backend's `measure_text`.

**Validation**: Add a `measure_text_consistency` test that prints text widths
from each backend and diffs them.

### Phase 2: Sokol Headless / Offscreen Capture

**Goal**: Sokol/Metal backend can render to an offscreen target and export PNG.

**What's missing**:
- `capture_render_texture()` -- not implemented
- `capture_screen_to_memory()` -- not implemented
- No headless mode (sokol_app always creates a window)

**Fix**:
1. Implement `capture_render_texture()` using `sg_query_image_pixeldata()` or
   Metal's `getBytes:` on the texture
2. For headless: use `sokol_app` with `sapp_desc.hidden = true` (if supported)
   or create an offscreen-only pass without a window

### Phase 3: Unified Shape Tessellation

**Goal**: Rounded rectangles and circles produce identical vertices.

**Fix**: Move tessellation logic to a shared `afterhours::shapes` module that
generates vertex lists. Each backend just submits the same vertices.

### Phase 4: Font Rasterization Parity

**Goal**: Same font file + same size = same glyph bitmaps.

This is the hardest phase. Options:
- **A**: Have both backends use fontstash/stb_truetype for rasterization
  (raylib already uses stb_truetype in `LoadFontEx`)
- **B**: Pre-rasterize a font atlas offline and load it identically in both
  backends
- **C**: Accept near-identical text rendering (within tolerance) and focus on
  layout parity

Recommendation: Start with **C** (tolerance-based), work toward **A**.

### Phase 5: Cross-Backend Validation Tool

**Goal**: Automated comparison of raylib vs metal output.

```bash
# Build
make cross_backend_validation_raylib
make cross_backend_validation_metal

# Run
./cross_backend_validation_raylib --output-dir=out/raylib -q
./cross_backend_validation_metal --output-dir=out/metal -q

# Compare
./cross_backend_compare out/raylib out/metal --tolerance=2
```

The comparison tool is backend-agnostic -- it just loads PNGs and diffs them.
The existing `compare_and_diff()` function can be extracted and reused.

---

## Priority Order

1. **Phase 2** (Sokol capture) -- Can't compare what we can't capture
2. **Phase 1** (Unified measurement) -- Layout parity is more important than
   pixel-perfect shapes
3. **Phase 5** (Comparison tool) -- Automate the comparison
4. **Phase 3** (Unified shapes) -- Nice to have
5. **Phase 4** (Font parity) -- Hardest, most diminishing returns

---

## Open Questions

- Should we support a `--backend=raylib,metal` flag in a single orchestrator?
- Do we need Windows/D3D backend validation?
- Should the cross-backend scenes use the full afterhours game loop
  (`run_game_loop`) or just render one frame?
- Is tolerance-based text matching acceptable long-term, or do we need
  bit-perfect text?
