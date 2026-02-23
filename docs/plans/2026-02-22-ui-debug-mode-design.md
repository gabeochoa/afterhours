# UI Debug Mode — Visual Debugging Toolkit

Analysis of [Loupe](https://github.com/Aeastr/Loupe) (SwiftUI debugging toolkit) and what we adopted for afterhours' UI debug mode.

**Goal:** Add runtime visual debugging tools that help developers understand layout, spot unnecessary renders, and verify alignment — without needing to read log output or use external tools.

---

## Reference Analysis: Loupe

Loupe is a SwiftUI package providing five debug tools, all conditionally compiled behind `#if DEBUG`:

### 1. debugRender() — Render Cycle Visualization

Each time a SwiftUI view is redrawn, a `Canvas` generates a random-hue color and fills the view's background. If the view isn't being redrawn, the color stays the same. This makes unnecessary re-renders immediately visible — you can see which parts of the tree are being touched each frame.

Implementation: A `DebugRender<Content: View>` wrapper that adds a `Canvas` background. The canvas generates `Color(hue: .random(in: 0...1), saturation: .random(in: 0.7...1), brightness: .random(in: 0.8...1))` with 0.4 opacity on every render.

### 2. debugCompute() — Re-initialization Detection

Separate from rendering: detects when a view's `init()` is called (meaning SwiftUI recreated the value type). Uses a `LocalRenderManager` (an `@ObservableObject`) that triggers a red flash overlay on init, then fades it out after 0.4 seconds via `DispatchQueue.main.asyncAfter`.

This distinguishes between "the view was redrawn" (debugRender) and "the view was re-created from scratch" (debugCompute).

### 3. VisualLayoutGuide — Bounds & Insets Overlay

A miniature (130x130pt) overlay panel placed on any view showing:
- View dimensions (width × height) with ruler indicators
- Safe area insets (top/leading/bottom/trailing) as colored bars with labels
- Optional label text
- Collision avoidance: an `OverlayPositionCoordinator` tracks all overlay frames and offsets colliding ones vertically with 8pt spacing

Features draggable positioning (via `DragGesture`) and persistence (saves offsets to `UserDefaults`). Uses `onGeometryChange` for efficient tracking.

### 4. VisualGridGuide — Alignment Grid

Draws a square grid overlay. Two modes:
- **Exact:** Calculates largest square size that divides both width and height evenly using GCD
- **Preferred:** Uses the requested square size, centers the grid, and reports remainder

Shows metrics in a corner badge (square size in points, grid dimensions, remainder for preferred mode). Uses `Canvas` for the grid lines and `displayScale` for crisp 1px lines.

### 5. DraggablePositionView — Position Tracker

A draggable circle with crosshairs that reports its x/y coordinates in a tooltip. Supports local, named, or global coordinate spaces. Includes drag constraints (horizontal only, vertical only, range limits) and position persistence.

---

## Existing Afterhours Debug Tools

Before this work, afterhours already had more validation infrastructure than Loupe, but less visual debugging:

### Validation Systems (`validation_systems.h`, `validation_config.h`)

Ten runtime validators with configurable severity (Silent/Warn/Strict):
- `ValidateScreenBounds` — elements stay within safe area margins
- `ValidateChildContainment` — children stay within parent bounds
- `ValidateComponentContrast` — WCAG AA contrast ratio check (text vs background)
- `ValidateMinFontSize` — minimum font size for readability
- `ValidateResolutionIndependence` — flags `Dim::Pixels` usage instead of `h720()`/`screen_pct()`/`percent()`
- `ValidateZeroSize` — catches elements that resolved to 0×0
- `ValidateAbsoluteMarginConflict` — flags margins on absolute-positioned elements
- `ValidateLabelHasFont` — elements with text have a font set
- `ValidateSpacingRhythm` — 4px spacing rhythm enforcement
- `ValidatePixelAlignment` — no fractional pixel positions

Violations are rendered as colored borders (red/orange/yellow by severity) via `RenderOverlay`.

### Layout Debug Overlay (`debug_overlay.h`)

`LayoutDebugOverlay<InputAction>` — toggle with a hotkey. Shows:
- Blue borders for Row flex direction
- Green borders for Column flex direction
- Orange borders for NoWrap elements
- Red highlights for overflow situations
- Small directional indicators in corners

### Layout Inspector (`layout_inspector.h`)

`LayoutInspector<InputAction>` — toggle with a hotkey. Side panel showing:
- Component name and entity ID
- Size, position
- Flex direction, justify content, align items, self-align, flex wrap
- Padding and margin (TRBL)
- Hierarchy (parent, children count)
- Overflow detection

Click a component to select it; hover for highlight.

### Render Debug (`rendering.h`)

`RenderDebugAutoLayoutRoots<InputAction>` — text-based tree view of all layout roots with entity IDs, positions, sizes, and component names. Supports node isolation (click to filter).

---

## Gap Analysis: What Loupe Has That We Didn't

| Loupe Feature | Afterhours Equivalent | Gap |
|---|---|---|
| debugRender (random color per render) | None | **Missing** — no way to see which elements are being touched each frame |
| debugCompute (red flash on re-init) | N/A | Not applicable — immediate-mode UI doesn't have view re-initialization |
| VisualLayoutGuide (inline dimensions) | LayoutInspector (side panel) | **Partial** — dimensions available but require click + side panel; not overlaid directly on elements |
| VisualGridGuide (alignment grid) | ValidateSpacingRhythm (log-based) | **Missing** — rhythm validated numerically but not visualized; no grid overlay |
| DraggablePositionView (coordinate tracker) | LayoutInspector hover highlight | **Partial** — coordinates shown in inspector but not as lightweight hover tooltip |

---

## What We Built: `ui_debug_mode.h`

New file at `src/plugins/ui/ui_debug_mode.h` — four render systems plus a status panel, all togglable via hotkeys.

### GridOverlay

Draws horizontal and vertical lines at a configurable spacing (default 8px) over the entire screen. Equivalent to Loupe's `VisualGridGuide` but adapted for our immediate-mode context.

Parameters: `spacing` (default 8px), `opacity` (default 0.15), `color` (default white).

Shows a legend at the bottom-left with the grid spacing value.

Use case: Visually verify that padding, margins, and component edges align to your spacing rhythm. Much faster than reading validation log output.

### RenderFlash

Each frame, every rendered `UIComponent` gets a semi-transparent overlay with a random hue (high saturation/brightness). Direct port of Loupe's `debugRender()`.

In immediate-mode UI, every component is "rendered" every frame, so this is most useful for:
- Seeing which components exist and where they are
- Spotting overlapping elements (colors blend)
- Identifying hidden/zero-size components (they won't flash)

Parameters: `flash_opacity` (default 0.3).

### InlineDimensionLabels

Overlays a `WxH` pill label at the top-left of every component that's at least 20×12 pixels. Uses a dark background pill for readability. Direct equivalent of Loupe's `VisualLayoutGuide` size display, but applied to every element simultaneously.

Parameters: `font_size` (default 10px), `label_opacity` (default 0.85).

Skips components too small to display the label. Clamps the pill within component bounds.

### HoverInspector

Lightweight alternative to the full `LayoutInspector` panel. Hover over any component to see a floating tooltip showing:
- Component name and entity ID
- Position (x, y)
- Size (width × height)
- Flex direction
- Padding (TRBL)
- Margin (TRBL)
- Children count

Uses smallest-area heuristic to select the deepest (most specific) component under the cursor. Highlights the hovered element with a blue outline.

Tooltip auto-positions near the cursor and flips to avoid going off-screen.

### DebugModePanel

Draws a thin "DEBUG MODE" status bar at the bottom of the screen when active. Serves as a visual reminder that debug overlays are enabled.

---

## API

```cpp
#include <afterhours/src/plugins/ui/ui_debug_mode.h>

// Option 1: Single hotkey toggles all debug features together
ui::debug_mode::register_render_systems<MyInput>(sm, MyInput::F7);

// Option 2: Separate hotkeys per feature
ui::debug_mode::register_render_systems<MyInput>(
    sm,
    MyInput::F7,    // master panel
    MyInput::F8,    // grid overlay
    MyInput::F9,    // render flash
    MyInput::F10,   // dimension labels
    MyInput::F11);  // hover inspector

// Option 3: Register individual systems
sm.register_render_system(
    std::make_unique<ui::debug_mode::GridOverlay<MyInput>>(
        MyInput::F8, 4.f));  // 4px grid

sm.register_render_system(
    std::make_unique<ui::debug_mode::HoverInspector<MyInput>>(
        MyInput::F11));
```

Register after your main UI render systems (`RenderImm` or `RenderBatched`) so the debug overlays draw on top.

---

## Architecture Decisions

**Why separate systems instead of a monolithic debug system?**
Each feature is independently toggleable with its own hotkey. This follows the existing pattern (`LayoutDebugOverlay`, `LayoutInspector`, `RenderDebugAutoLayoutRoots` are all separate systems with separate toggles). Users can register only the features they want.

**Why `SystemWithUIContext<UIComponent>` base?**
Same base class used by `LayoutDebugOverlay` and `LayoutInspector`. Iterates over all entities with `UIComponent`, which is what we need for debug visualization.

**Why inline HSV→RGB instead of adding to `colors::`?**
Keeps the debug mode self-contained. The HSV conversion is only needed for the random-hue render flash effect. If other systems need it later, it can be promoted to `color.h`.

**Why not Loupe's `debugCompute()` equivalent?**
Afterhours uses immediate-mode UI — there's no concept of "view re-initialization" to detect. Every component is rebuilt from scratch each frame by design. The render flash already covers the useful case (seeing what's being drawn).

---

## Comparison with Existing Debug Tools

| Tool | Activation | Shows | Interaction |
|---|---|---|---|
| `LayoutDebugOverlay` (F4) | Toggle | Flex direction, wrap, overflow borders | View only |
| `LayoutInspector` (F5) | Toggle + click | Full property dump in side panel | Click to select |
| `RenderDebugAutoLayoutRoots` (F3) | Toggle | Text tree of all components | Click to isolate |
| `ValidationOverlay` | Config flag | Violation borders (red/orange/yellow) | View only |
| **`GridOverlay`** (new) | Toggle | Spacing rhythm grid | View only |
| **`RenderFlash`** (new) | Toggle | Random-color overlays per component | View only |
| **`InlineDimensionLabels`** (new) | Toggle | W×H on every component | View only |
| **`HoverInspector`** (new) | Toggle + hover | Tooltip with layout details | Hover |

The new tools fill the "visual at-a-glance" gap. The existing tools are better for deep investigation (LayoutInspector) and automated enforcement (validation systems). They complement each other.
