# Afterhours UI Plugin

Immediate-mode UI toolkit built on the Afterhours ECS framework. All widgets are created each frame using builder-pattern `ComponentConfig` and positioned via a flexbox-like auto-layout system.

## Quick Start

```cpp
#include <afterhours/src/plugins/ui.h>
#include <afterhours/src/plugins/ui/text_input/text_input.h>

using namespace afterhours::ui;
using namespace afterhours::ui::imm;

// In your render system:
void for_each_with(Entity &entity, UIContext<InputAction> &ctx, float) override {
  if (button(ctx, mk(entity, 0),
      ComponentConfig{}.with_label("Click Me").with_size({pixels(200), pixels(50)}))) {
    // clicked
  }
}
```

## Widgets

### Layout

| Widget | Header | Description |
|--------|--------|-------------|
| `div` | `imm_components.h` | Container / text label. Defaults to `children()` sizing. |
| `separator` | `imm_components.h` | Horizontal or vertical line. Optional center label (`"--- OR ---"` style). |
| `scroll_view` | `imm_components.h` | Scrollable container. Mouse wheel input, content clipping. |
| `decorative_frame` | `imm_components.h` | Decorative border. Styles: `KraftPaper`, `Simple`, `Inset`. |

### Buttons & Selection

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

### Data Input

| Widget | Header | Description |
|--------|--------|-------------|
| `slider` | `imm_components.h` | Draggable slider (0.0–1.0). Keyboard left/right support. Optional handle label. |
| `dropdown` | `imm_components.h` | Dropdown select menu. Absolute-positioned options list. |
| `text_input` | `text_input/component.h` | Single-line text input. Cursor, selection, clipboard, password masking. |
| `text_area` | `text_input/text_area.h` | Multiline text editor. Word wrap, vertical scrolling, line height config. |

### Navigation

| Widget | Header | Description |
|--------|--------|-------------|
| `tab_container` | `imm_components.h` | Horizontal tab row. Equal-width tabs, active highlighting. |
| `navigation_bar` | `imm_components.h` | `< Label >` navigation with arrow buttons. |
| `pagination` | `imm_components.h` | Page selector with `< 1 2 3 >` buttons. |

### Display

| Widget | Header | Description |
|--------|--------|-------------|
| `progress_bar` | `imm_components.h` | Read-only progress bar. Label styles: `Percentage`, `Fraction`, `Custom`, `None`. |
| `circular_progress` | `imm_components.h` | Radial progress ring. Configurable thickness, fill/track colors. |
| `image` | `imm_components.h` | Static image display. |
| `sprite` | `imm_components.h` | Spritesheet sprite with source rectangle. |
| `icon_row` | `imm_components.h` | Row of spritesheet icons with uniform scaling. |

### Composite

| Widget | Header | Description |
|--------|--------|-------------|
| `setting_row` | `setting_row.h` | Label + control row for settings screens. Control types: `Toggle`, `Stepper`, `Slider`, `Display`, `Dropdown`. |

## Overlay Plugins

These are top-level plugins (not in `ui/`) that provide overlay UI systems:

| Plugin | Header | Description |
|--------|--------|-------------|
| `modal` | `modal.h` | Modal dialogs. Helpers: `modal::info()`, `modal::confirm()`, `modal::fyi()`. Stacking, focus trapping, input blocking, `ClosedBy` modes. |
| `toast` | `toast.h` | Toast notifications. Levels: `Info`, `Success`, `Warning`, `Error`, `Custom`. Auto-dismiss, queue, animation. |

## ComponentConfig

All widgets accept a `ComponentConfig` with a chainable builder API. Below is every available method.

### Content & Identity

```cpp
.with_label("text")              // Display text
.with_debug_name("name")         // Debug identifier (shows in hierarchy viewer)
.with_render_layer(N)            // Z-order layer (higher = on top)
.with_internal(true)             // Mark as internal (skip in public queries)
```

### Sizing

```cpp
.with_size({pixels(200), pixels(50)})     // Fixed pixel size
.with_size({percent(0.5f), percent(1.0f)}) // Relative to parent
.with_size({screen_pct(0.3f), h720(40)})  // Relative to screen
.with_size({children(), children()})       // Shrink to fit content
.with_size({expand(1.0f), pixels(50)})     // Fill remaining space (flex-grow)
```

**Size functions:**

| Function | Description |
|----------|-------------|
| `pixels(N)` | Fixed pixel size |
| `percent(0.5f)` | Fraction of parent (0–1) |
| `screen_pct(0.3f)` | Fraction of screen height |
| `h720(40)` | Pixels at 720p baseline, scales with resolution |
| `w1280(100)` | Pixels at 1280p baseline, scales with resolution |
| `children()` | Shrink-wrap to content |
| `expand(weight)` / `flex_grow(weight)` | Fill remaining space proportionally |

### Layout

```cpp
.with_flex_direction(FlexDirection::Row)          // Row or Column
.with_justify_content(JustifyContent::Center)     // Main axis: FlexStart, FlexEnd, Center, SpaceBetween, SpaceAround
.with_align_items(AlignItems::Center)             // Cross axis: FlexStart, FlexEnd, Center, Stretch
.with_self_align(SelfAlign::Center)               // Override parent's align_items for this element
.with_flex_wrap(FlexWrap::NoWrap)                 // Prevent wrapping
.with_no_wrap()                                   // Shorthand for NoWrap
.with_clip_children(true)                         // Scissor-clip overflowing children
.with_padding(Spacing::md)                        // Uniform padding (xs/sm/md/lg/xl)
.with_padding(Padding{.top=pixels(8), ...})       // Per-side padding
.with_margin(Spacing::sm)                         // Uniform margin
.with_margin(Margin{.bottom=DefaultSpacing::small()}) // Per-side margin
```

### Positioning

```cpp
.with_absolute_position()          // Remove from flow, position manually
.with_translate(100.0f, 200.0f)    // Offset in pixels (after layout)
.with_translate(h720(100), h720(200)) // Resolution-scaled offset
```

### Colors & Background

```cpp
.with_background(Theme::Usage::Primary)   // Use theme color
.with_custom_background(Color{r,g,b,a})   // Fixed color
.with_text_color(Theme::Usage::Accent)     // Theme text color
.with_custom_text_color(Color{...})        // Fixed text color
.with_auto_text_color(true)                // Auto white/black for contrast (default: true)
```

**Theme::Usage values:** `Primary`, `Secondary`, `Accent`, `Surface`, `Background`, `Foreground`, `Error`, `None`, `Custom`, `Default`

### Typography

```cpp
.with_font("FontName", h720(24))           // Font name + responsive size
.with_font("FontName", 24.0f)              // Font name + fixed pixel size
.with_alignment(TextAlignment::Center)      // Left, Center, Right
```

### Borders & Shadows

```cpp
// Borders
.with_border(Color{...}, 2.0f)                   // Solid border
.with_bevel(BevelStyle::Raised, light, dark, 1.0) // Bevel border
.with_nine_slice_border(texture, slice_size)       // 9-slice texture border

// Box shadows
.with_shadow(ShadowStyle::Soft, ox, oy, blur, color)
.with_hard_shadow(4.0f, 4.0f, Color{0,0,0,120})
.with_soft_shadow(4.0f, 6.0f, 12.0f, Color{0,0,0,60})

// Text effects
.with_text_stroke(Color{0,0,0,255}, 2.0f)    // Text outline
.with_text_shadow(Color{0,0,0,128}, 2.0f, 2.0f) // Text drop shadow
.with_soft_text_shadow()                       // Subtle text shadow
.with_hard_text_shadow()                       // Strong text shadow
```

### Rounded Corners

```cpp
.with_rounded_corners(std::bitset<4>(0b1111))      // All corners
.with_rounded_corners(RoundedCorners().left_round()) // Left only
.with_rounded_corners(RoundedCorners().all_sharp())  // No rounding
.with_roundness(0.5f)                                // 0.0 = sharp, 1.0 = fully round
.with_segments(16)                                   // Arc segment count
.disable_rounded_corners()
```

### Visual Modifiers

```cpp
.with_opacity(0.5f)       // 0.0 = invisible, 1.0 = fully opaque
.with_scale(1.1f)         // Visual scale (after layout, for animations)
.with_hidden(true)        // Hide element
.with_disabled(true)      // Disable interaction (grayed out)
```

### Textures

```cpp
.with_texture(texture_handle)                          // Attach texture
.with_texture(texture_handle, HasTexture::Alignment::Center) // With alignment
.with_image_alignment(HasTexture::Alignment::Stretch)  // Image fit mode
```

### Interaction

```cpp
.with_skip_tabbing(true)                      // Skip in tab order
.with_select_on_focus(true)                   // Auto-select on keyboard focus
.with_click_activation(ClickActivationMode::Press) // Click on press vs release
```

### Text Input Specific

```cpp
.with_mask_char('*')           // Password masking
.with_line_height(pixels(18))  // Text area line height
.with_word_wrap(true)          // Enable word wrapping
.with_max_lines(5)             // Max line count (0 = unlimited)
.with_checkbox_indicators("v", " ")     // Custom checked/unchecked symbols
.with_dropdown_indicators("v", "^")     // Custom open/closed symbols
```

### Animation (Declarative)

```cpp
.with_animation(Anim::on_click().scale(0.9f, 1.0f).spring())
.with_animation(Anim::on_appear().opacity(0.0f, 1.0f).ease_out(0.3f))
.with_animation(Anim::on_hover().translate_y(0.0f, -4.0f).ease_out(0.15f))
.with_animation(Anim::loop().scale(0.95f, 1.05f).ease_in_out(1.0f))
```

**Triggers:** `on_click()`, `on_appear()`, `on_hover()`, `on_focus()`, `loop()`
**Properties:** `.scale(from, to)`, `.translate_x(from, to)`, `.translate_y(from, to)`, `.opacity(from, to)`
**Curves:** `.spring()`, `.ease_out(duration)`, `.ease_in(duration)`, `.ease_in_out(duration)`, `.linear(duration)`

### Prebuilt Config Styles

```cpp
magazine_style()    // Generous padding/margin, base typography
card_style()        // Medium padding, small margin
form_style()        // Compact padding, tiny margin
auto_spacing()      // Applies automatic defaults
```

## Theme System

```cpp
Theme theme;
theme.primary = Color{...};
theme.secondary = Color{...};
theme.accent = Color{...};
theme.background = Color{...};
theme.surface = Color{...};
theme.font = Color{...};
theme.font_muted = Color{...};
theme.roundness = 0.08f;
theme.focus = Color{...};

// Per-language font config
theme.language_fonts[Language::Korean] = FontConfig{"KoreanFont", 0.95f};

// Font sizing (auto-interpolates missing values)
theme.font_sizing.medium = 20.0f;
theme.font_sizing.finalize();

// Apply to context
ctx.theme = theme;
```

## Color Utilities

```cpp
#include <afterhours/src/plugins/color.h>

colors::darken(color, 0.8f);
colors::lighten(color, 0.2f);
colors::lerp(color_a, color_b, t);
colors::opacity_pct(color, 0.5f);
colors::with_alpha(color, 128);
colors::auto_text_color(background);  // Returns white or black for best contrast
colors::hex(0x3498DB);
colors::rgb(52, 152, 219);
colors::rgba(52, 152, 219, 200);
```

## Other Plugins

### Command History

```cpp
#include <afterhours/src/plugins/command_history.h>

CommandHistory<MyState> history;
history.execute(state, std::make_unique<MyCommand>(...));
history.undo(state);
history.redo(state);
history.can_undo();  // bool
history.can_redo();  // bool
```

### Translation

```cpp
#include <afterhours/src/plugins/translation.h>

using namespace afterhours::translation;
// Language enum: English, Korean, Japanese
// TranslatableString<ParamEnum> with fmt-based parameter substitution
```

### Clipboard

```cpp
#include <afterhours/src/plugins/clipboard.h>

afterhours::clipboard::set_text("copied text");
std::string text = afterhours::clipboard::get_text();
```

### Animation (Key-based)

```cpp
#include <afterhours/src/plugins/animation.h>

// See main README.md for full animation API documentation
afterhours::animation::anim(MyKey::FadeIn)
  .from(0.0f).to(1.0f, 0.3f, EasingType::EaseOutQuad);
float v = afterhours::animation::manager<MyKey>().get_value(MyKey::FadeIn);
```

## Entity Management

```cpp
mk(parent, index)  // Create EntityParent from parent entity + child index
mk(parent)          // Auto-incrementing index
deref(ep_pair)       // Destructure into [entity, parent]
```

## E2E Testing

```cpp
#include <afterhours/src/plugins/e2e_testing/e2e_testing.h>
#include <afterhours/src/plugins/e2e_testing/test_input.h>

// Input injection
test_input::push_key(KEY_A);
test_input::set_mouse_position({100, 200});
test_input::simulate_mouse_button_press(MOUSE_BUTTON_LEFT);
test_input::simulate_tab();
test_input::simulate_enter();
test_input::simulate_escape();
```

## Debug Tools

- `debug_overlay.h` — bounds overlay, hover inspector
- `layout_inspector.h` — hierarchy viewer
- `validation_config.h` / `validation_systems.h` — config validation warnings

## File Index

```
plugins/
  ui.h                    # Umbrella include (backward compat)
  modal.h                 # Modal dialog system
  toast.h                 # Toast notifications
  animation.h             # Key-based animation manager
  command_history.h        # Undo/redo
  color.h                 # Color utilities
  translation.h           # i18n
  clipboard.h             # System clipboard
  window_manager.h         # Resolution/FPS
  input_system.h           # Input mapping
  texture_manager.h        # Sprites/textures
  ui/
    component_config.h     # ComponentConfig builder
    imm_components.h       # All immediate-mode widgets
    setting_row.h          # Setting row composite widget
    animation_config.h     # Declarative animation (Anim builder)
    animation_keys.h       # Animation state components
    component_init.h       # Internal: component initialization
    components.h           # UI component types and state
    context.h              # UIContext (focus, hot, active state)
    element_result.h       # ElementResult return type
    entity_management.h    # mk(), deref()
    layout_types.h         # Size, Dim, FlexDirection, Spacing, etc.
    styling_defaults.h     # UIStylingDefaults per-component config
    theme.h                # Theme, FontSizing, FontConfig
    rendering.h            # Render systems
    render_primitives.h    # Low-level draw calls
    rounded_corners.h      # RoundedCorners builder
    systems.h              # Update/render system registration
    utilities.h            # is_mouse_inside, etc.
    debug_overlay.h        # Debug visualization
    layout_inspector.h     # Hierarchy viewer
    validation_config.h    # Config validation rules
    validation_systems.h   # Config validation systems
    ui_core_components.h   # Core UI components
    text_input/
      text_input.h         # Umbrella include + docs
      component.h          # text_input() widget
      text_area.h          # text_area() widget
      text_area_state.h    # Multiline state
      state.h              # HasTextInputState
      storage.h            # TextStorage concept
      selection.h          # TextSelection
      line_index.h         # LineIndex (offset <-> row/col)
      text_layout.h        # TextLayoutCache (word wrap)
      utils.h              # UTF-8 utils, cursor, word nav
      systems.h            # Text editing ECS systems
      concepts.h           # C++20 concepts
  e2e_testing/
    e2e_testing.h          # Umbrella include
    test_input.h           # Input injection
    input_injector.h       # Low-level injection
    runner.h               # Test runner
    command_handlers.h     # DSL command handlers
    ui_commands.h          # UI-specific commands
    visible_text.h         # Text visibility tracking
    concepts.h             # C++20 concepts
    pending_command.h      # Command queue
```
