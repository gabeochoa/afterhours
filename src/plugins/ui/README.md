# Afterhours UI Plugin

Immediate-mode UI toolkit built on the Afterhours ECS framework. All widgets are created each frame using builder-pattern `ComponentConfig` and positioned via a flexbox-like auto-layout system.

## Quick Start

```cpp
#include <afterhours/src/plugins/ui.h>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

struct MyUI : System<DefaultUIContext> {
  void for_each_with(Entity &entity, DefaultUIContext &ctx, float) override {
    if (button(ctx, mk(entity, 0), "Click Me")) {
      // clicked
    }
  }
};

int main() {
  SystemManager systems;
  ui::setup<>(systems, std::make_unique<MyUI>());
  systems.run(1.f);
}
```

`ui::setup()` creates the UI root and every singleton, then registers the
update systems in the only order that works with your systems in between. It
defaults `InputAction` to `ui::DefaultAction`, so nothing above declares an
enum. Use `setup_with_resolution()` to pass a starting resolution; if your app
already registered `ProvidesCurrentResolution`, setup defers to it.

For finer control the pieces are still public. `init_ui_plugin()`,
`register_before_ui_updates()`, `register_after_ui_updates()`.

### Input actions

Every widget system is templated on an `InputAction` enum and looks its actions
up by name, so a custom enum must define all 26 values the plugin references
before it will compile. `ui::DefaultAction` ships that vocabulary; supply your
own only when you want UI actions fused with your game bindings.

## Widgets and configuration

| Widget | Header | Description |
|--------|--------|-------------|
| `div` | `imm_components.h` | Container / text label. Defaults to `children()` sizing. |
| `vsplit` / `hsplit` | `imm_components.h` | Divide a region into N parts and return all N at once. N is deduced from the size list. |
| `separator` | `imm_components.h` | Horizontal or vertical line. Optional center label (`"--- OR ---"` style). |
| `div` + `with_overflow` | `imm_components.h` | Scrollable/clipped container via `config.with_overflow(Overflow::Scroll, Axis::Y)`. |
| `decorative_frame` | `imm_components.h` | Decorative border. Styles: `KraftPaper`, `Simple`, `Inset`. |

| Widget | Header | Description |
|--------|--------|-------------|
| `button` | `imm_components.h` | Clickable button. Returns `true` on click. |
| `button_group` | `imm_components.h` | Row or column of equally-sized buttons. Returns clicked index. |
| `checkbox` | `imm_components.h` | Checkbox with label. Toggles a `bool&`. |
| `checkbox_no_label` | `imm_components.h` | Checkbox without label. |
| `checkbox_group` | `imm_components.h` | Group of checkboxes backed by `std::bitset`. Optional min/max selection count. |
| `radio_group` | `imm_components.h` | Single-select radio buttons with circular indicators. |
| `toggle_switch` | `imm_components.h` | iOS-style toggle. Styles: `Pill` (sliding knob) or `Circle` (checkmark/X). |
| `image_button` | `imm_components.h` | Clickable image/sprite. |

| Widget | Header | Description |
|--------|--------|-------------|
| `slider` | `imm_components.h` | Draggable slider (0.0–1.0). Keyboard left/right support. Optional handle label. |
| `dropdown` | `imm_components.h` | Dropdown select menu. Absolute-positioned options list. |
| `text_input` | `text_input/component.h` | Single-line text input. Cursor, selection, clipboard, password masking. |
| `text_area` | `text_input/text_area.h` | Multiline text editor. Word wrap, vertical scrolling, line height config. |

| Widget | Header | Description |
|--------|--------|-------------|
| `tab_container` | `imm_components.h` | Horizontal tab row. Equal-width tabs, active highlighting. |
| `navigation_bar` | `imm_components.h` | `< Label >` navigation with arrow buttons. |
| `pagination` | `imm_components.h` | Page selector with `< 1 2 3 >` buttons. |

| Widget | Header | Description |
|--------|--------|-------------|
| `progress_bar` | `imm_components.h` | Read-only progress bar. Label styles: `Percentage`, `Fraction`, `Custom`, `None`. |
| `circular_progress` | `imm_components.h` | Radial progress ring. Configurable thickness, fill/track colors. |
| `image` | `imm_components.h` | Static image display. |
| `sprite` | `imm_components.h` | Spritesheet sprite with source rectangle. |
| `icon_row` | `imm_components.h` | Row of spritesheet icons with uniform scaling. |

| Widget | Header | Description |
|--------|--------|-------------|
| `setting_row` | `setting_row.h` | Label + control row for settings screens. Control types: `Toggle`, `Stepper`, `Slider`, `Display`, `Dropdown`. |

| Plugin | Header | Description |
|--------|--------|-------------|
| `modal` | `modal.h` | Modal dialogs. Helpers: `modal::info()`, `modal::confirm()`, `modal::fyi()`. Stacking, focus trapping, input blocking, `ClosedBy` modes. |
| `toast` | `toast.h` | Toast notifications. Levels: `Info`, `Success`, `Warning`, `Error`, `Custom`. Auto-dismiss, queue, animation. |

| Function | Description |
|----------|-------------|
| `pixels(N)` | Fixed pixel size |
| `percent(0.5f)` | Fraction of parent (0–1) |
| `screen_pct(0.3f)` | Fraction of screen height |
| `h720(40)` | Pixels at 720p baseline, scales with resolution |
| `w1280(100)` | Pixels at 1280p baseline, scales with resolution |
| `children()` | Shrink-wrap to content |
| `expand(weight)` / `flex_grow(weight)` | Fill remaining space proportionally |

## Composition and lifetime

`vsplit`/`hsplit` return regions from a list of Sizes; the cross axis fills the
container and expand() uses leftover main-axis space. Splits fill their parent
by default. Configure the parent normally; use `restyle` for generated regions.

`restyle` overlays explicitly set fields during the build pass. It cannot express
unset-vs-false for plain booleans or layout enums. Rebuild the base config each frame;
do not retain an element handle across frames for later styling. For widgets you
create directly, branch on ordinary ComponentConfig values instead.

Use stable `mk` identities and public component APIs. Distinguish logical Size values
from resolved pixels, and label wrapping from flex wrapping. Default flex is NoWrap.
Explicit font sizes, actual font variants and insets must agree between layout and paint.

See [optional plugin APIs](../../../docs/plugins.md) for dialogs, prompts and charts,
[profiling](../../../docs/profiling.md) for the panel, and the
[backlog](../../../todo.md) for limitations. The root
[quick start](../../../README.md#quick-start) includes a window/event loop.
