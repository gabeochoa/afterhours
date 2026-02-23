# Styling Presets — Lessons from PanGui's Immediate-Mode Styling

Analysis of [PanGui's styling blog post](https://pangui.io/blog/03-immediate-mode-styling/) and what ideas apply to afterhours' UI system.

**Goal:** Improve the reusability, consistency, and visual polish of styled UI components without introducing an external stylesheet language.

---

## Reference Analysis: PanGui

PanGui is a C# immediate-mode UI library that ships a full CSS-like styling language on top of its IMGUI core. The styling system is parsed from text (loaded from strings, files, or functions), supports hot reloading, and runs at roughly 3-5x the cost of hard-coded visuals — which at their scale (0.05ms → 0.15ms for 1000 elements) is negligible.

### Key Features

#### 1. External Stylesheets with CSS-like Selectors

Styles are loaded from text and matched to elements by tag name. Selectors support nesting and direct child combinators, similar to CSS:

```
button { ... }
toolbar > button { ... }
dialog button.danger { ... }
```

Elements get a single "tag" identifier rather than multiple classes. The tag hierarchy is derived from entering/exiting style nodes during the IMGUI pass.

#### 2. Modifiers (Interaction State Overrides)

Built-in modifiers (`:hover`, `:hold`, `:focus`) and custom modifiers each define their own transition duration and easing. When a modifier activates, all properties it overrides are smoothly interpolated:

```
button {
    solidColor(gray);
    :hover {
        solidColor(white);
        transition: 200ms ease-out;
    }
    :hold {
        scale: 0.95;
        transition: 80ms ease-out;
    }
}
```

Custom modifiers can be activated from IMGUI code by pushing modifier tags, allowing app-specific states (e.g., `:selected`, `:error`, `:loading`) with automatic transitions.

#### 3. Style Inheritance

Three flavors:
- `#inherit(box)` — copies all properties from `box` AND matches `box` selectors
- `#inherit-properties(box)` — copies properties only (no selector matching)
- `#inherit-selector(box)` — matches `box` selectors only (no property copying)

Multiple inheritance is supported. This lets you build a hierarchy like `warning-box` inherits from `box` inherits from `panel`.

#### 4. Variables

Styles can declare variables that modifiers animate. Built-in variables include `$width`, `$height`, `$time`, `$cursor-position`. User code can also inject variables from the IMGUI side.

Variables can be used in shape/effect expressions, enabling data-driven animations without code changes.

#### 5. SDF Shapes and Effects

Styles define visuals through composable SDF shape primitives (circle, rect, roundedRect, arc, triangle, etc.) with boolean operations (union, intersect, subtract) and transforms (move, rotate, scale). Effects (solidColor, linearGradient, radialGradient, outerShadow, innerShadow, stroke) are applied to shapes in order.

#### 6. Macros

Text-substitution macros with parameter support:
- `@const` — immutable values
- `@mixin` — reusable style content blocks
- `@style` — parameterized style generators
- `@shape` / `@effect` — reusable graphics

#### 7. Separation of Logic and Visuals

Core philosophy: a single `Button()` function works for toolbar buttons, tab buttons, large buttons, etc. The stylesheet controls all visual differences. The IMGUI code only deals with identity, state, and layout intent.

#### 8. Error Reporting and Debugging

Styling errors are shown as runtime overlays in the app. A debugger panel lets you inspect which style properties were applied to a node, where they came from, and the layout hierarchy — similar to browser developer tools.

---

## Current Afterhours Approach

### What We Have

| Feature | Mechanism | Location |
|---------|-----------|----------|
| Per-element styling | `ComponentConfig` builder pattern | `component_config.h` |
| Global theme | `Theme` struct with color palette | `theme.h` |
| Per-component-type defaults | `UIStylingDefaults` singleton with `ComponentType` enum | `styling_defaults.h` |
| Declarative animations | `Anim` builder (triggers: click, hover, focus, appear, loop) | `animation_config.h` |
| Visual decorators | `ElementResult::decorate()` with factory lambdas | `ui_decorators.h`, `element_result.h` |
| Interaction states | Implicit hover highlighting in render system | `rendering.h` |

### What Works Well

- **`ComponentConfig` builder** is ergonomic and discoverable. Chained methods make it easy to configure any aspect of a component inline.
- **`Theme`** provides consistent color palettes with auto-generated derived colors and WCAG accessibility validation.
- **`Anim` builder** is elegant for simple animations (`Anim::on_hover().scale(1.05f).ease_out()`).
- **Decorators** compose well for visual additions (brackets, grid backgrounds, blockquotes).

### Gaps Identified

1. **No reusable style bundles.** Every button call carries its full visual config inline. If you want 20 toolbar buttons to look the same, you repeat the config 20 times or manually extract a function.

2. **No interaction-state-aware styling.** You can animate scale/opacity/translate on hover, but you can't say "on hover, change the border color to blue and the shadow to 8px blur." Colors, borders, and shadows change instantly.

3. **Fixed `ComponentType` enum for defaults.** You can set defaults for `Button`, `Checkbox`, etc., but not for custom categories like "toolbar_button", "card_header", or "ghost_button".

4. **No smooth color/border/shadow transitions.** When hover state changes, the background color jumps. There's no lerp between normal and hover colors.

5. **No style inheritance.** You can't say "danger_button is like toolbar_button but with a red background." `apply_overrides()` exists but is manual and flat.

---

## Proposed Improvements

### 1. StylePreset — Reusable Styles with State Overrides

A `StylePreset` bundles a base `ComponentConfig` with per-state overrides and transition parameters. When the component's interaction state changes, the system automatically interpolates between configs.

```cpp
struct StyleTransition {
  float duration = 0.15f;
  AnimCurve curve = AnimCurve::EaseOut;
};

struct StylePreset {
  ComponentConfig base;

  struct StateOverride {
    ComponentConfig config;
    StyleTransition transition;
  };

  std::optional<StateOverride> hover;
  std::optional<StateOverride> pressed;
  std::optional<StateOverride> focused;
  std::optional<StateOverride> disabled;

  // Builder API
  class Builder;
  static Builder create();
};
```

**Usage:**

```cpp
auto toolbar_btn = StylePreset::create()
    .base(ComponentConfig{}
        .with_custom_background(colors::gray(40))
        .with_roundness(0.2f)
        .with_border(colors::gray(80), 1.0f))
    .on_hover(ComponentConfig{}
        .with_custom_background(colors::gray(60))
        .with_border(colors::white, 1.0f),
        {.duration = 0.15f, .curve = AnimCurve::EaseOut})
    .on_press(ComponentConfig{}
        .with_custom_background(colors::gray(30))
        .with_scale(0.97f),
        {.duration = 0.08f})
    .on_disabled(ComponentConfig{}
        .with_opacity(0.4f));

// At the call site — clean and consistent
button(ctx, mk(parent), toolbar_btn.with_label("Save"));
button(ctx, mk(parent), toolbar_btn.with_label("Load"));
button(ctx, mk(parent), toolbar_btn.with_label("Export"));
```

**Key design decisions:**
- `StylePreset` is a value type, not a registry entry. You can create them anywhere.
- The `with_label()` / `with_size()` etc. methods return a copy with the override applied, so the preset itself is immutable and reusable.
- State overrides use `apply_overrides()` semantics — only explicitly set fields take effect.

### 2. Style Registry — Named Styles

A global registry for looking up styles by string name. Useful for themes, game-specific styles, and reducing coupling between code that defines styles and code that uses them.

```cpp
struct StyleRegistry {
  static StyleRegistry &get();

  void register_style(const std::string &name, const StylePreset &preset);
  std::optional<StylePreset> get_style(const std::string &name) const;
  bool has_style(const std::string &name) const;

private:
  std::unordered_map<std::string, StylePreset> styles_;
};

// Convenience function
inline ComponentConfig style(const std::string &name) {
  auto preset = StyleRegistry::get().get_style(name);
  return preset ? preset->resolve_for_state(/* current state */)
                : ComponentConfig{};
}
```

**Registration example:**

```cpp
// In game startup / theme initialization
auto &reg = StyleRegistry::get();
reg.register_style("toolbar_button", toolbar_btn);
reg.register_style("card_header", StylePreset::create()
    .base(ComponentConfig{}
        .with_custom_background(theme.surface)
        .with_font_size(FontSize::Large)
        .with_padding(Spacing::md)));
```

**Relationship to existing `UIStylingDefaults`:**
- `UIStylingDefaults::set_component_config()` continues to work for the built-in `ComponentType` enum.
- `StyleRegistry` is for user-defined named styles — it's additive, not a replacement.
- A future iteration could unify them (component-type defaults become named presets internally).

### 3. Auto-Interpolating Visual Transitions

When the render system detects a state change (hover enter/exit, focus gain/loss, press/release), it should lerp visual properties over the transition duration specified by the `StylePreset`.

**Properties to interpolate:**
- Background color (RGBA lerp)
- Border color and thickness
- Shadow offset, blur, and color
- Opacity
- Scale
- Roundness

**Implementation sketch:**

```cpp
// Per-entity component tracking interpolation state
struct HasStyleTransition : BaseComponent {
  Color current_bg;
  Color target_bg;
  float current_border_thickness;
  float target_border_thickness;
  // ... other interpolatable properties

  float elapsed = 0.0f;
  float duration = 0.15f;
  AnimCurve curve = AnimCurve::EaseOut;
  bool is_active = false;
};
```

A system would update these each frame, applying eased interpolation. The render system would read from `HasStyleTransition::current_*` instead of the raw config values.

**Relationship to existing `Anim` system:**
- `Anim` handles transform-like properties (scale, translate, opacity, rotation) — keep it.
- `HasStyleTransition` handles visual appearance properties (color, border, shadow) — new.
- They complement each other. A hover could trigger both an `Anim` scale bounce and a smooth color transition.

### 4. Style Inheritance / Composition

Allow a `StylePreset` to extend another, overriding only what's different:

```cpp
auto danger_btn = StylePreset::extend(toolbar_btn)
    .base(ComponentConfig{}
        .with_custom_background(colors::red))
    .on_hover(ComponentConfig{}
        .with_custom_background(colors::lighten(colors::red, 0.2f)));

auto ghost_btn = StylePreset::extend(toolbar_btn)
    .base(ComponentConfig{}
        .with_transparent_bg()
        .with_custom_text_color(theme.font));
```

**Implementation:** `extend()` deep-copies the parent preset, then applies overrides on top. Simple and predictable — no runtime resolution chain.

---

## What We're NOT Adopting

| PanGui Feature | Reason to Skip |
|----------------|----------------|
| External stylesheet files / text parser | Massive effort, doesn't fit C++ header-only architecture. Builder pattern in code is sufficient. |
| SDF shape system | Orthogonal to our quad-based rendering. Would require a shader pipeline. |
| Expression language for variables | C++ lambdas and the existing `Anim` system cover this. |
| Macro system (@const, @mixin, etc.) | C++ templates, constexpr, and helper functions already serve this role. |
| Tag-based selector matching | Over-engineered for our use case. Named presets are simpler and more explicit. |
| Hot reloading stylesheets | Requires file watching and a parser. Not practical for our architecture. |

---

## Priority and Effort Estimates

| Item | Value | Effort | Dependencies |
|------|-------|--------|--------------|
| `StylePreset` struct with state overrides | High | Low | None |
| `StylePreset::Builder` fluent API | High | Low | StylePreset |
| `StyleRegistry` (named styles) | High | Low | StylePreset |
| Style inheritance via `extend()` | Medium | Low | StylePreset |
| `HasStyleTransition` component | High | Medium | StylePreset |
| Transition update system | High | Medium | HasStyleTransition |
| Render system integration (read from transition state) | High | Medium | Transition system |
| Migrate existing `UIStylingDefaults` to use presets internally | Low | Low | StylePreset, Registry |

**Suggested order:** StylePreset → Builder → Registry → extend() → HasStyleTransition → transition system → render integration.

---

## Architectural Notes

### Integration with Existing Systems

- **`ComponentConfig`** remains the core styling primitive. `StylePreset` composes configs, it doesn't replace them.
- **`Theme`** continues to define the color palette. Presets reference theme colors via `Theme::Usage`.
- **`Anim`** continues to handle transform animations. Transitions handle appearance animations.
- **`UIStylingDefaults`** can optionally be backed by `StylePreset` internally in a future iteration.

### Performance Considerations

- `StylePreset` is a value type with no runtime overhead beyond the configs it contains.
- `StyleRegistry` lookups are `O(1)` hash map access — negligible.
- `HasStyleTransition` adds per-entity state only for components that use presets with transitions. Components without presets have zero overhead.
- The transition update system runs only on entities with active transitions (dirty flag pattern).

### Migration Path

This is purely additive. No existing code needs to change. Existing `ComponentConfig`-based styling continues to work. Teams can adopt `StylePreset` incrementally — one component type at a time.

---

## Source

- Blog post: [Immediate mode styling in PanGui](https://pangui.io/blog/03-immediate-mode-styling/) (October 2025)
- PanGui is a C# IMGUI library by Sirenix ApS with a custom stylesheet language
- Analysis date: February 22, 2026
