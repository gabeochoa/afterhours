# TODO List

> **Regeneration Instructions for Cursor Agents:**
>
> To update this file with current TODOs from the codebase, follow these steps:
>
> 1. **Search for TODO comments:**
>    ```bash
>    grep -rn --include="*.h" --include="*.cpp" --include="*.c" --include="*.hpp" --exclude-dir="vendor" "TODO\|FIXME\|HACK\|XXX" vendor/afterhours/src/
>    ```
>
> 2. **For each TODO found:**
>    - Read the surrounding code context (5-10 lines) to understand the issue
>    - Categorize as: Bug, Performance, Feature, Architecture, Documentation, etc.
>    - Assign priority: High (bugs/safety), Medium (performance/arch), Low (features)
>    - Write clear explanation of what the TODO actually means and why it exists
>
> 3. **Organize by:**
>    - Core System (library, bitset, developer)
>    - Plugins (input, UI subsections, E2E testing, texture, autolayout, modal)
>    - Backend-specific (sokol, etc.)
>
> 4. **Include summary section with:**
>    - Priority breakdown (High/Medium/Low/Informational)
>    - Statistics (total TODOs, by category)
>
> 5. **Exclude vendor directory** - contains third-party code TODOs we don't control

---

This document contains all TODO comments found in `vendor/afterhours/src/`, analyzed with context to understand the actual issues.

## Core System TODOs

### Library (`src/library.h`)
- **Line 101**: **Feature — Random Generator** — Random generator is stubbed out; always uses index 0 instead of a proper random selection from matched results

### Bitset Utilities (`src/bitset_utils.h`)
- **Line 90**: **Code Quality — Combine Functions** — Two related utility functions should be merged

### Developer Tools (`src/developer.h`)
- **Line 68**: **Architecture — Code Organization** — Developer tools should be moved to a dedicated file

## Plugin TODOs

### Color (`src/plugins/color.h`)
- **Line 177**: **Architecture — Vector Type Abstraction** — Consider using `#ifdef VECTOR_TYPE` to avoid hardcoding vector type

### Texture Manager (`src/plugins/texture_manager.h`)
- **Line 52**: **Code Quality — Duplicated Code** — Code was copied from transform component; should be refactored to share
- **Line 88**: **Feature — Text Alignment** — Need support for `InnerLeft` and `InnerRight` alignment options

### AutoLayout (`src/plugins/autolayout.h`)
- **Line 575**: **Configuration — Fallback Behavior** — Questioning if fallback behavior should be a configurable setting
- **Line 716**: **Layout Logic — Flex Children** — Unsure about applying sizing logic to non-1.0 flex children
- **Line 812**: **Layout Constraint** — Cannot enforce size assertions when text wrapping is enabled

### Modal (`src/plugins/modal.h`)
- **Line 47**: **Architecture — Config Integration** — Should use `ComponentConfig` for modal configuration
- **Line 317**: **Feature — Configuration Support** — Add support for modal configuration options

### Input System (`src/plugins/input_system.h`)
- **Line 106-109**: **Informational — macOS Workaround** — `raylib::IsMouseButtonPressed` is broken on macOS because `glfwSwapBuffers` pumps the Cocoa event queue, setting `currentButtonState` before `PollInputEvents` copies current→previous. Manual edge detection via `IsMouseButtonDown` is used instead. (Active workaround, not a TODO to fix.)
- **Line 236**: **Feature — Controller Support** — Currently only using Xbox controller button names; need PlayStation and other controller icons
- **Line 272**: **Feature — macOS Icon** — Need macOS-specific icon for super/command key
- **Line 482**: **Informational (Raylib Upstream)** — `KEY_MENU` maps to the same value as `KEY_R`. Raylib 5.5 adds `KEY_MENU` for Android; potential upstream conflict.
- **Line 643**: **Feature — Mouse Position** — `get_mouse_position()` is not implemented ("good luck")
- **Line 772**: **Feature — Configurable Deadzone** — Gamepad deadzone (0.25) is hardcoded; should be user-configurable
- **Line 894**: **Architecture — Singleton Query** — `get_input_collector()` should use a singleton query pattern
- **Line 904**: **Architecture — Namespace** — Input system struct should be moved out of the `input` namespace

### E2E Testing

#### Input Injector (`src/plugins/e2e_testing/input_injector.h`)
- **Line 10**: **Architecture — Input Parity** — E2E input injector should match 1-to-1 with the UI input system to share code

#### Pending Command (`src/plugins/e2e_testing/pending_command.h`)
- **Line 111**: **Design Decision — Error Handling** — Should pending command failures throw exceptions or auto-fail?

#### Runner (`src/plugins/e2e_testing/runner.h`)
- **Line 129**: **Feature — Plugin Registration** — Add a way for plugins to register their own e2e testing commands

#### UI Commands (`src/plugins/e2e_testing/ui_commands.h`)
- **Line 66**: **Feature — Conversion Helper** — Add a `gen_lambda()` helper for entity conversion
- **Line 367**: **Feature — Slider Calculation** — Calculate percentage from slider min/max when `HasSliderState` supports it
- **Line 410**: **Feature — Wait + Click** — Add wait and option-click logic for e2e tests

### UI System

#### Context (`src/plugins/ui/context.h`)
- **Lines 3-9**: **Architecture — C++20 Concepts** — Consider using C++20 concepts for type constraints (see `e2e_testing/concepts.h` for examples)
- **Line 69**: **Architecture — InputBitset Coupling** — `InputBitset` definition should move to input system; currently creates a dependency on `magic_enum` in the UI layer

#### Theme (`src/plugins/ui/theme.h`)
- **Line 61**: **Architecture — Font Identification** — Investigate using a `FontID` enum instead of strings for type safety

#### Component Config (`src/plugins/ui/component_config.h`)
- **Lines 29-36**: **Architecture — Config Splitting** — Consider splitting monolithic `ComponentConfig` into concept-constrained configs per component type (e.g., `TextInputConfig` only exposes text-input-relevant methods)
- **Line 87**: **Design Decision — Inheritance** — Should all component config properties be inheritable?
- **Line 548**: **Code Quality — Rename** — Rename method to `is_absolute()` for clarity

#### Components (`src/plugins/ui/components.h`)
- **Line 114**: **Architecture — State Unification** — Consider unifying `HasStepperState` and `HasDropdownState` since a stepper is essentially a dropdown variant
- **Line 520**: **Code Quality — Magic Numbers** — Build more confidence around how to set numeric values to avoid issues
- **Line 534**: **Feature — Drag Groups** — Consider adding named drag groups and accept-list filtering

#### Entity Management (`src/plugins/ui/entity_management.h`)
- **Line 60**: **Feature — Element Tracking** — Add a count of how many UI elements are created

#### Rendering (`src/plugins/ui/rendering.h`)
- **Line 337**: **Performance — Caching** — Rendering system needs caching for better performance
- **Line 1444**: **Note** — Self-referential note indicating TODO might not be needed

#### Core Components (`src/plugins/ui/ui_core_components.h`)
- **Line 263**: **Configuration — Default Values** — Uncertain about default spacing values (5,5 vs 10,10)

#### Systems (`src/plugins/ui/systems.h`)
- **Line 69-70**: **Architecture — Wrong Module** — Function should live inside `input_system` but doing so would require `magic_enum` as a dependency there
- **Line 279**: **Architecture — System Filter** — Should move logic to a system-level filter
- **Line 367**: **Architecture — Tag Support** — Template approach works but wishes it worked better with Tags without requiring `UIComponent` in `for_each_with`
- **Line 771**: **Feature — Repeat Rate** — Consider using a different key repeat rate for WidgetLeft
- **Line 811**: **Feature — Repeat Rate** — Consider using a different key repeat rate (duplicate context)
- **Line 926**: **Architecture — Side Effects** — Figure out if current approach will actually cause trouble
- **Line 955**: **Architecture — Pound Define** — Replace magic number validation (options > 100) with a `#define`
- **Line 1177**: **Performance — Inlining** — Consider inlining drag-tag query helpers
- **Line 1200**: **Feature — Gen For Each** — Need a `gen_for_each()` or equivalent for tag operations
- **Line 1244**: **Feature — Deep Clone** — Only flat properties (HasLabel, HasColor) are copied during drag. Dragged items with children won't render correctly; consider deep-cloning subtree
- **Line 1549**: **Architecture — Tag Conversion** — Consider converting `was_rendered_to_screen` to a tag
- **Line 1552**: **Code Quality — Combine Checks** — Combine `should_hide` and `ShouldHide` tag checks
- **Line 1581**: **Feature — Natural Scroll** — Add support for customizing scroll direction ("natural" scroll)

#### Immediate Mode Components (`src/plugins/ui/imm_components.h`)
- **Line 65**: **Architecture — Namespace** — Consider moving existing primitives (div, button, sprite, image) into a `primitives` namespace
- **Line 67**: **Architecture — Namespace** — Consider whether stateful convenience wrappers should live in a separate namespace
- **Line 497**: **Code Quality — Button Wrapping Hack** — Current approach to get buttons to wrap is a hack; needs a cleaner solution
- **Line 1123**: **Bug — Slider Overflow** — `slider_background` can overflow by ~1-2px when parent is constrained
- **Line 1222**: **Feature — Slider Handle Height** — Support custom handle height via a dedicated config field
- **Line 1448**: **Architecture — Hot Sibling** — Summary of previous label-checkbox interaction behavior that changed
- **Line 1536**: **Feature — Tag Setter** — Add a way to set tags directly from a bool
- **Line 1549**: **Architecture — Navigation Bar** — Consider making `navigation_bar` a thin wrapper around existing components
- **Line 1578**: **Feature — Default Values** — Add defaults for configuration
- **Line 2304**: **Feature — Neighbor Styling** — Make neighbor styling configurable (muted color, smaller font, etc.)

#### Text Input (`src/plugins/ui/text_input/component.h`)
- **Line 227**: **Feature — Horizontal Scrolling** — Implement horizontal scrolling when text exceeds field width

#### Setting Row (`src/plugins/ui/setting_row.h`)
- **Line 295**: **Architecture — Reusable Component** — Add a component for this instead of building one inline

#### Styling Defaults (`src/plugins/ui/styling_defaults.h`)
- **Line 101**: **Architecture — Singleton Helper** — Needs a singleton helper pattern

## Backend TODOs

### Sokol Drawing Helpers (`src/backends/sokol/drawing_helpers.h`)
- **Line 209**: **Feature — Rotation Support** — Drawing helpers lack rotation support
- **Line 338**: **Feature — Thick Lines** — Implement thick lines via quads

---

## Summary by Priority

### High Priority (Bugs)
- **Slider background overflow** (`src/plugins/ui/imm_components.h:1123`) — can overflow by 1-2px when parent is constrained

### Informational (Upstream / Active Workarounds)
- **macOS IsMouseButtonPressed workaround** (`src/plugins/input_system.h:106-109`) — raylib bug; manual edge detection is the active fix
- **KEY_MENU / KEY_R conflict** (`src/plugins/input_system.h:482`) — raylib upstream issue

### Medium Priority (Architecture & Performance)
- **Random generator stubbed out** (`src/library.h:101`)
- **Font identification uses strings** (`src/plugins/ui/theme.h:61`)
- **Monolithic ComponentConfig** (`src/plugins/ui/component_config.h:29-36`)
- **Function in wrong module** (`src/plugins/ui/systems.h:69-70`)
- **C++20 concepts modernization** (`src/plugins/ui/context.h:3-9`)
- **InputBitset dependency coupling** (`src/plugins/ui/context.h:69`)
- **Plugin e2e command registration** (`src/plugins/e2e_testing/runner.h:129`)
- **Various systems.h architectural items** (`src/plugins/ui/systems.h:926,955,1177,1200,1244`)

### Low Priority (Features & Code Quality)
- Code quality: rename suggestions, DRY violations, namespace organization, hack cleanups
- Features: texture alignment options, element creation tracking, stepper/dropdown unification, drag groups, horizontal text scrolling, natural scroll, slider handle customization, neighbor styling, rotation/thick-line drawing
- Architecture: config inheritance semantics, namespace restructuring, modal config integration, singleton patterns

**Total TODOs Found**: 65
**High Priority**: 1
**Informational**: 2
**Medium Priority**: 8
**Low Priority**: 54
**Last Updated**: February 2026

---

# Ergonomics: "Shortest Hello World" Showcase

> **NOT derived from source TODO comments — do not delete during regeneration.**
> Hand-written from a review of [ratatui](https://docs.rs/ratatui/latest/ratatui/)'s API.

## Goal

Build `examples/hello/main.cpp` — a showcase that is as short as ratatui's hello
world while keeping the full customization surface available. Ratatui's is 10
lines:

```rust
fn main() -> std::io::Result<()> {
    ratatui::run(|mut terminal| {
        loop {
            terminal.draw(|frame| frame.render_widget("Hello World!", frame.area()))?;
            if event::read()?.is_key_press() { break Ok(()); }
        }
    })
}
```

Ours today (`examples/catalog/ui/ui_component/main.cpp`) is ~60 lines: a 5-value
`InputAction` enum, 18 lines of singleton wiring, `enforce_singletons`, and four
systems that must be registered in an exact order. The items below are what
closes that gap. Each is additive — no existing call site changes.

The showcase is the acceptance test: if hello world isn't short, the item isn't
done. Each item lands with its own line in `examples/hello/main.cpp`.

## Items (ordered by diff size, smallest first)

### 1. `ComponentConfig` implicit from string — **DONE**

Ratatui renders a bare `&str` as a widget. Ours needs
`ComponentConfig{}.with_label("Hello")`.

`ComponentConfig` is currently an aggregate with no constructors. All 135
existing uses are `ComponentConfig{}` or copy-init from another config
(`imm_components.h:2388,2449`), so adding constructors breaks nothing:

```cpp
ComponentConfig() = default;
ComponentConfig(std::string_view l) : label(l) {}  // implicit on purpose
```

Unlocks `button(ctx, mk(e), "Click Me")` and `div(ctx, mk(e), "Hello")`.

Landed with a `const char*` overload alongside the `string_view` one — without
it, `const char*` → `string_view` → `ComponentConfig` is two user-defined
conversions and the language won't do it implicitly.

### 2. Expose `min_size` / `max_size` on `ComponentConfig` — **DONE**

Ratatui makes `Min` and `Max` first-class constraints resolved *above*
`Length`/`Percentage`/`Fill` — it's how you express "sidebar at most 300px" or
"content at least 100px".

`UIComponent` already has `min_size`/`max_size` with setters
(`ui_core_components.h:80-82`, `:231-246`) but `ComponentConfig` never exposes
them, so no imm caller can reach it. This is finishing an existing feature, not
adding one: `AutoLayout::apply_size_constraints` (`autolayout.h:741`) already
does the work.

Landed as four per-axis methods — `with_min_width` / `with_max_width` /
`with_min_height` / `with_max_height` — rather than `ComponentSize` pairs.
Constraining one axis is the common case, and `with_max_width(pixels(300))`
beats a pair with a placeholder in the other slot. Add pair versions if a real
call site wants them.

Covered by `tests/size_constraints_test.cpp`. The check that matters is
`unset_constraints_do_not_clamp`: `apply_layout` calls the setters
unconditionally, so a default `Dim::None` must pass through instead of clamping
everything to zero.

### 3. `vsplit` / `hsplit` — destructured layout split — **DONE**

Ratatui's best ergonomic:

```rust
let [title, main, status] = Layout::vertical([Length(1), Min(0), Length(1)]).areas(frame.area());
```

Ours needs three `div`s, three `mk()`s, three configs, and manual `.ent()`
threading. All the machinery already exists (`expand()`, `pixels()`, `vstack`,
`mk(parent, index)`); what's missing is the function returning
`std::array<ElementResult, N>`:

```cpp
auto [title, main, status] = imm::vsplit<3>(ctx, mk(entity),
                                            {pixels(30), expand(1), pixels(30)});
```

Landed taking `const Size (&)[N]` rather than `std::array<Size, N>`, so N is
deduced from the braced list and there is no `<3>` to drift out of sync with a
three-element list. Regions are built inside a pack expansion because
`ElementResult` holds a reference and so is neither default-constructible nor
assignable; braced-init-list expansion is guaranteed left-to-right, which is
what keeps the returned regions in declared order.

Splits fill their parent by default, unlike `vstack`/`hstack`, which
shrink-wrap on the stacking axis — a split divides a region it has been given.

Covered by `tests/split_test.cpp` (tiling, expand weights, nesting, deduction)
and demoed headless by `examples/catalog/ui/split_layout/`.

### 4. `ui::setup<InputAction>()` — one-call initialization — **DONE**

Ratatui's `run()` handles setup, teardown, and panic hooks so the app author
writes none of it.

**Correction from building item 3:** most of this already exists and the gap is
smaller than first written. `ui::init_ui_plugin<InputAction>()`
(`utilities.h:97`) already creates the root and every singleton, and
`register_before_ui_updates` / `register_after_ui_updates` already wrap the
system ordering in bridges. What is left is genuinely small:

```cpp
ui::setup<InputAction>(systems, std::make_unique<MyUISystem>());
```

collapsing init + before + user + after into one call, and folding in
`ProvidesCurrentResolution`, which `RunAutoLayout` requires but
`init_ui_plugin` does not create — every app had to remember it separately.

Landed as `ui::setup()` plus `ui::setup_with_resolution()`, both variadic over
the caller's systems. The resolution singleton is only added when nothing else
registered one, so apps driving a real window can call
`window_manager::add_singleton_components()` first and setup defers. The
underlying pieces stay public for apps that need custom ordering.

Note the catalog examples do **not** use this path; several still hand-roll the
old `ClearUIComponentChildren` → `BeginUIContextManager` → `EndUIContextManager`
wiring and never run layout at all.

### 4b. Example catalog build rot — **DONE**

All 49 catalog examples compile again (was 4). Their includes said
`../../../src/...`, one level short of where they live since the category
directories were added; re-rooted to `../../../../`. Two makefiles needed the
same one-level fix in `SRCS`/`-I`.

`common.mk` gained a `build` target so the catalog can be compiled without
running each example — needed because sweeping with `all` hangs on the
long-running ones.

Fixing the build surfaced failures that had been dark the whole time the
examples did not compile:

- `core/entity_query` — asserted `take(2) == 3`, documenting the off-by-one
  that commit a3e16a2 fixed. Stale expectation, corrected.
- `ui/ui_layout` — 30 of 56 cases, 108 assertions. **Fixed; all 289 now
  pass.** Every one was the same stale expectation: commit d41e1d8 fixed
  margins wrongly shrinking elements ("pixels(70) with 24px margin rendered
  as 46px"), moving to the CSS model where margin is external. This suite
  encoded the pre-fix behaviour and went dark before it could notice.
  Verified as a pure semantics shift before touching anything — positions
  unchanged, and the margin total implied by old and new expectations agreed
  on every one of the 30 cases. **Zero real layout bugs.**

`safety/benchmarks` and `ui/layout_performance` exceed a 15s timeout, which is
expected for those two — not failures.

### 5. `ui::DefaultAction` — stop forcing an enum declaration — **DONE**

Ratatui aliases `DefaultTerminal` specifically "so you rarely spell out backend
generics." We thread `InputAction` through every UI type.
`examples/imm_visual_check.cpp` declares **26 enum values verbatim** — the full
vocabulary the nav/text/debug systems reference — just to compile.

Confirmed while writing `examples/catalog/ui/split_layout`: a 5-value enum is
enough for the old hand-rolled wiring, but the moment you use the real
`register_after_ui_updates` bridge the compiler demands `WidgetLeft`,
`WidgetRight`, `WidgetUp`, `WidgetDown`, `MenuBack` and the whole `Text*` set.
That example carried the same 26-value block, which was the single largest
thing standing between it and a short hello world.

Landed as `ui::DefaultAction` in `context.h` with exactly the 26 values the
library references, plus `using DefaultUIContext = UIContext<DefaultAction>`.
`tests/setup_test.cpp` pins the list — the failure mode without it is an app
that suddenly stops compiling with no hint that the default set regressed.

Skipped the default keymap: binding actions to keys is the app's job and every
backend spells key codes differently. Add one if a real app wants it.

Ship that vocabulary as `ui::DefaultAction` plus
`using DefaultUIContext = UIContext<DefaultAction>`, with a default keymap.
Hello world then declares no enum at all; games still supply their own.

### 6. Serializable `Theme` (stretch)

Ratatui gates a `serde` feature on its style types explicitly for theme files.
Our `Theme` is hardcoded C++, so every color tweak is a recompile. Not strictly
an ergonomics item, but it's the iteration-speed one — and a hot-reloading theme
is the most compelling thing a showcase can demo.

### 6b. Split region configs — deferred

`vsplit`/`hsplit` could take `{size, config}` pairs so regions are styled where
they are declared:

```cpp
auto [title, body] = vsplit(ctx, mk(e), {
  {pixels(36),  ComponentConfig{}.with_custom_background(navy)},
  {expand(1.f), ComponentConfig{}.with_custom_background(dark)},
});
```

`Region` would be implicitly constructible from `Size`, so the terse form keeps
working and N stays deduced.

Not done because `restyle()` already covers it, and the two differ only in
where the config sits — locality vs one uniform mechanism. Worth revisiting if
splits start carrying a lot of per-region styling and the `restyle()` calls
drift far from the `vsplit()` they belong to.

Note for whoever picks this up: **dynamic styling was never the reason** for
either feature. Configs are values, so `with_custom_background(x ? a : b)`
already covers that. The actual gap was reaching elements the caller did not
create — split regions, `progress_bar`'s track/fill, `dropdown`'s options.

### 7. modal::confirm overflows below 720p

Found while chasing layout warnings. `create_confirm_content` hardcodes
`ModalConfig{}.with_size(h720(540), h720(230))` (`modal.h:568`) and `confirm()`
exposes no size override, so callers cannot work around it.

`h720` scales the modal box with resolution but the message text does not
scale with it, so below 720p the box shrinks while its content does not and
the button row spills out of the dialog:

```
Layout overflow: 'dialog_buttons' extends outside parent 'modal' bounds
  (child_size=[386.0,46.7], child_end=[386.0,170.0], parent_size=[386.0,143.7])
```

Reproduce by registering a `ProvidesCurrentResolution` of 800x600 in
`ImmTestHarness` and running `dialog_test`. The harness does not register one
today, so the modal's `resolve_size` falls back to a 1280x720 baseline, the box
comes out full size, and the overflow is hidden. That fallback is itself worth
fixing — the harness reports 800x600 through `UIContext` and lays out at
800x600, so the resolution lookup disagreeing with both is a test-fidelity gap.
Fixing it needs this modal issue solved first, otherwise `dialog_test` just
goes noisy.

Likely fix: size the dialog to its content (`children()` height, or a min
height) instead of a fixed `h720`, and/or let `confirm()` take a `ModalConfig`.

## Explicitly not doing

- **Layout-solve caching / draw diffing.** Ratatui caches the constraint solve
  (`layout-cache`) and diffs terminal cells because writing to a terminal is
  brutally expensive. On a GPU that inverts: our draw is cheap and the layout
  solve is the cost. Immediate mode rebuilds the tree every frame, so cache hit
  rate is the entire question. **Profile before writing any of it.**
- **Renaming the `with_` prefix off `ComponentConfig` methods.** Ratatui's
  `Stylize` shorthand (`.red().on_white().bold()`) is nice, but ~60 renames of
  churn across every call site for saved keystrokes is a bad trade.
- **A `Widget` trait.** Ratatui needs `impl Widget for Foo`; our free functions
  returning `ElementResult` already compose identically with less ceremony.
- **A `prelude` namespace.** `ui.h` plus the three `using namespace` lines
  already covers it.

## Already at parity or ahead

- `JustifyContent` covers ratatui's `Flex::{Start,Center,SpaceBetween,SpaceAround}`.
- `ElementResult::decorate()` has no ratatui equivalent and is nicer than their
  `Block::bordered()` wrapping.
- `mk()`'s source-location hashing gives stable widget identity for free;
  ratatui pushes state management entirely onto the caller
  (`render_stateful_widget`).
