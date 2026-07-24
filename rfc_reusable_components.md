# RFC: Reusable UI Components (afterhours)

**Status:** Draft · **Target:** `src/plugins/ui/`

## Summary

Make it ergonomic to build, name, theme, and share **reusable UI components** on top of
afterhours' immediate-mode UI. Today you can override colors via a `Theme` and per-type
defaults via `UIStylingDefaults`, but authoring a *new* reusable widget means copying a
60+ line free function full of magic numbers, manual `init_state`, and hand-wired theme
lookups — and there is no way to give that widget a *name* the theme/defaults system
recognizes. This RFC proposes three additive layers:

1. **Presets** — named, themeable `ComponentConfig` bundles (zero new machinery; pure ergonomics).
2. **Component recipes** — a small, declared way to register a user widget so it participates
   in theming/defaults/validation like the built-ins, without editing the core `ComponentType` enum.
3. **Composition helpers** — reduce the boilerplate in a widget author's function body
   (state, focus, sub-element parenting, magic-number extraction).

All three are **opt-in and backwards-compatible**. Existing `button(...)`/`toggle_switch(...)`
call sites and the current `Theme`/`UIStylingDefaults` API keep working unchanged.

## Motivation

### What works today
- `Theme` (theme.h): a `Usage`-keyed color palette (`Primary`, `Surface`, `Accent`, `Error`, …)
  plus font sizing/stroke/shadow. Good.
- `UIStylingDefaults` (styling_defaults.h): per-`ComponentType` default `ComponentConfig`
  (`set_component_config`, `merge_with_defaults`). Good — but keyed on a **closed enum**.
- `ComponentConfig`: a 75-method builder (`with_background`, `with_border`, `with_font_size`, …).
  Powerful, but verbose to repeat.
- `ComponentConfig::inherit_from(parent, name)`: the one existing composition primitive.

### The friction (grounded in the code)
1. **No user-nameable widget identity.** Styling defaults are keyed by `enum ComponentType`
   (`Button`, `Slider`, `ToggleSwitch`, …, `Tray`). A user who builds a "Card" or "StatPill"
   cannot register defaults/theme for it without editing the library's enum. There is no
   `ComponentType::Custom("Card")`.
2. **Widget authoring = copy a big function.** `toggle_switch` (imm_components.h) is ~120 lines:
   hardcoded `track_w = 52.0f, track_h = 28.0f, pad = 4.0f`, manual `init_state<HasToggleSwitchState>`,
   manual `FocusClusterRoot`/`InFocusCluster` wiring, manual `Theme::Usage` color pulls, manual
   label sub-`div` with width math. To make a *variant* you fork the whole thing.
3. **Repeating a "look" is copy/paste.** Want every "danger button" to be red + bold + a specific
   border? Today you re-type the same `.with_background(...).with_border(...).with_font_tier(...)`
   at every call site, or wrap it in your own free function that the theme system doesn't know about.
4. **No sharing story.** There's no convention for "here's a component pack; drop it in and its
   widgets are themed by my app's Theme automatically."

## Goals
- Author a reusable, themed widget with far less boilerplate than copying `toggle_switch`.
- Give user widgets a **string identity** that the theme/defaults/validation systems honor,
  without touching the core `ComponentType` enum.
- Make "define a look once, reuse everywhere" a one-liner (`preset`).
- Keep everything **header-only, opt-in, and backwards-compatible**.

## Non-goals
- A retained-mode / declarative markup layer (afterhours is immediate-mode; keep it).
- Replacing `Theme` or `ComponentConfig` (we build *on* them).
- A runtime plugin/DLL system for widgets (they're just headers the app includes).
- Styling hot-reload / an editor (possible later, out of scope here).

## Design

### Layer 1 — Presets (named ComponentConfig bundles)

A **preset** is a named, reusable `ComponentConfig` transform. It composes with the existing
builder and theme; it adds no new rendering path.

```cpp
// Define once (app setup):
ui::register_preset("danger_button", [](ComponentConfig c) {
  return c.with_color_usage(Theme::Usage::Error)
          .with_font_tier(FontSizing::Tier::Medium)
          .with_border(Border::all(2.f))
          .with_bevel(Bevel::Raised);
});

// Use anywhere — .with_preset() applies it, caller overrides still win:
if (button(ctx, mk(entity),
      ComponentConfig{}.with_preset("danger_button").with_label("Delete"))) {
  ...
}
```

Semantics:
- `with_preset(name)` records the preset name; `init_component` applies registered presets
  **before** the caller's explicit `with_*` calls so explicit settings override the preset
  (predictable precedence: theme defaults < component-type defaults < preset < explicit call).
- Presets are just `std::function<ComponentConfig(ComponentConfig)>` stored in a registry keyed
  by string. Zero new component types, zero rendering changes.
- Multiple presets compose: `.with_preset("card").with_preset("elevated")` apply in order.

This alone removes ~80% of the "repeat the same look" pain with almost no new machinery.

### Layer 2 — Component recipes (nameable, themeable user widgets)

Extend the styling identity from a closed enum to an **open key** so user widgets are
first-class to theming/defaults/validation.

```cpp
// styling identity becomes: built-in enum  OR  a user string.
struct ComponentKey {
  ComponentType builtin = ComponentType::Div;
  std::string custom;                 // non-empty => user widget
  bool is_custom() const { return !custom.empty(); }
};
```

`UIStylingDefaults::component_configs` changes from `map<ComponentType, ComponentConfig>` to
`map<ComponentKey, ComponentConfig>` (ordering/hash by (builtin, custom)). Built-ins are
unaffected (custom is empty). Now:

```cpp
// Register a "Card" widget's default look, themed like a built-in:
UIStylingDefaults::get().set_component_config(
    ComponentKey::custom("Card"),
    ComponentConfig{}
      .with_color_usage(Theme::Usage::Surface)
      .with_border(Border::all(1.f))
      .with_rounded_corners(8.f)
      .with_margin(DefaultSpacing::small()));
```

A **recipe** bundles (a) the custom key, (b) its default config, and (c) the author's build
function, so dropping in a component pack wires up its theming automatically:

```cpp
namespace mypack {
inline ElementResult card(HasUIContext auto &ctx, EntityParent ep,
                          ComponentConfig config = {}) {
  // resolve_config merges: theme < custom-key defaults < preset < caller config
  auto cfg = ui::resolve_config(ComponentKey::custom("Card"), std::move(config));
  auto [e, parent] = deref(ep);
  init_component(ctx, ep, cfg, ComponentType::Div, /*interactive*/false, "card");
  return {true, e};
}

// One call registers defaults + validation so the pack is themed by the app's Theme:
inline void register_components() {
  ui::register_recipe("Card", card_defaults(), /*validation*/ {});
}
} // namespace mypack
```

Key property: **user widgets get the same theme/default/validation treatment as built-ins**
without ever editing the library's `ComponentType` enum. The enum stays for the built-ins;
`custom` strings extend it openly.

### Layer 3 — Composition helpers (shrink the widget-author function body)

The `toggle_switch` body shows the repetitive parts. Provide small helpers so a new widget
is mostly *its* logic, not plumbing:

```cpp
ElementResult toggle_switch(HasUIContext auto &ctx, EntityParent ep,
                            bool &value, ComponentConfig config = {}) {
  // 1. one call: resolve config + init container + focus cluster + row layout
  auto w = ui::begin_widget(ctx, ep, ComponentKey::custom("ToggleSwitch"),
                            std::move(config), {.row = true, .focus_cluster = true});

  // 2. state via a typed helper (no manual init_state<>)
  auto &st = w.state<HasToggleSwitchState>(value);

  // 3. named constants pulled from the widget's config, not magic numbers
  const auto m = w.metrics();  // track_w/track_h/pad/knob from config or preset

  // ... the actual toggle drawing/animation (the part that's genuinely unique) ...
  return w.result(clicked);
}
```

`begin_widget` folds together `resolve_config` (Layer 2), `init_component`, `FocusClusterRoot`,
and row/column setup — the four things every composite currently hand-rolls. `metrics()`
replaces per-widget magic numbers with values that live in the config/preset (and can be themed).

## Precedence (single, documented rule)
Lowest → highest, last wins:
```
Theme palette  <  component-key defaults (UIStylingDefaults)  <  preset(s)  <  explicit .with_* on the call
```
This is the one mental model an author needs. `resolve_config` implements exactly this order.

## Examples

### A. "Define a look once" (Layer 1 only — no core changes)
```cpp
ui::register_preset("pill", [](ComponentConfig c){
  return c.with_rounded_corners(999.f).with_margin(DefaultSpacing::tiny());
});
button(ctx, mk(e), ComponentConfig{}.with_preset("pill").with_label("Tag"));
```

### B. A themed custom "StatPill" widget shared as a pack (Layers 1–3)
```cpp
namespace hud {
inline ElementResult stat_pill(HasUIContext auto &ctx, EntityParent ep,
                               std::string_view text, ComponentConfig config = {}) {
  auto w = ui::begin_widget(ctx, ep, ComponentKey::custom("StatPill"),
                            std::move(config), {.row = true});
  label(ctx, mk(w.entity()), ComponentConfig{}.with_label(std::string(text)));
  return w.result();
}
inline void register_components() {
  ui::register_recipe("StatPill",
    ComponentConfig{}.with_color_usage(Theme::Usage::Accent)
                     .with_rounded_corners(999.f)
                     .with_font_tier(FontSizing::Tier::Small), {});
}
}
// App: hud::register_components();  then stat_pill(ctx, mk(e), "HP 42");
// It's themed by whatever Theme the app set — no per-call styling.
```

### C. Variant via preset instead of forking the function
```cpp
// No new function; just a preset over the built-in toggle:
ui::register_preset("big_toggle", [](ComponentConfig c){
  return c.with_size({pixels(300), pixels(56)});
});
toggle_switch(ctx, mk(e), muted, ComponentConfig{}.with_preset("big_toggle"));
```

## Migration / rollout (incremental, each shippable alone)
1. **Layer 1 (Presets)** — add registry + `with_preset` + apply-in-`init_component`. No core type
   changes. Immediately useful. (Small PR.)
2. **Layer 2a (ComponentKey)** — widen `UIStylingDefaults` map key from `ComponentType` to
   `ComponentKey`; built-ins pass `{builtin}`. Behavior-identical for existing code (test-gated).
3. **Layer 2b (recipes)** — `register_recipe` + `resolve_config` using the widened map.
4. **Layer 3 (begin_widget/metrics)** — refactor ONE built-in (e.g. `toggle_switch`) onto it as
   the proof, keeping output pixel-identical (screenshot/behavior gate), then offer it to authors.
5. Document in `src/plugins/ui/README.md` under a new "Authoring reusable components" section.

## Alternatives considered
- **Just extend the `ComponentType` enum per widget:** doesn't scale, requires library edits for
  every app widget, merge conflicts. Rejected — the whole point is user extensibility.
- **String-only styling (drop the enum):** breaks existing per-type defaults + is slower
  (string hashing hot path). `ComponentKey` keeps the enum fast-path and adds strings only when used.
- **A macro DSL for widgets:** hides control flow, hard to debug, un-afterhours-like. Rejected in
  favor of plain functions + small helpers.
- **Retained/declarative tree:** a different framework. Out of scope.

## Open questions
- `ComponentKey` map ordering vs hashing — do we need `unordered_map` for perf, or is the
  per-frame lookup count low enough that `std::map` is fine? (Measure before choosing.)
- Should presets be global or scoped to a `UIContext`? (Global is simpler; scoped enables
  per-screen theming.)
- Do recipes need validation hooks (min touch target, contrast) like built-ins, or is that
  opt-in per recipe?
- `metrics()` design: a fixed struct vs a small typed bag the widget declares.

## Backwards compatibility
- All new APIs are additive. No existing call site changes.
- `UIStylingDefaults` map-key widening is source-compatible for built-in usage (implicit
  `ComponentType → ComponentKey`).
- No change to `Theme`, `ComponentConfig`'s existing methods, or rendering.

## Performance
- Presets: one `std::function` call per element that opts in; negligible.
- `ComponentKey` lookup: same as today for built-ins (enum fast path); custom adds a string
  compare only for widgets that use it. Measure the map choice before finalizing (open question).
