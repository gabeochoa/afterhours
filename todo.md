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

## Goal — **DONE** (see item 8)

Build a showcase that is as short as ratatui's hello world while keeping the
full customization surface available. Landed as
`examples/catalog/ui/hello/main.cpp` at 17 lines. Ratatui's is 10:

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
done.

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

Originally skipped the default keymap on the grounds that "every backend spells
key codes differently." **That was wrong** — `afterhours::keys` is already a
backend-neutral set of GLFW/raylib codes. Shipped in item 8 below.

### 6. Serializable `Theme` — **DONE**

Ratatui gates a `serde` feature on its style types explicitly for theme files.
Ours was hardcoded C++, so every color tweak was a recompile.

Landed as `src/plugins/ui/theme_io.h`. Format is flat `key = value` with `#`
comments — no dependency. nlohmann/json is only reachable through the settings
plugin's opt-in macros and is not vendored, so JSON would have meant requiring
a dependency to change a color.

The field list is written once and drives both directions:

- **Colors** iterate `Theme::Usage` and go through `color_ref`, so a usage
  added to the enum is serialized without touching `theme_io`.
- **Scalars** use a member-pointer table (`float_fields`), so save and load
  cannot drift apart.

`HotReloadTheme` is a plain `System<>` the app registers, rather than something
baked into `run()`:

```cpp
ui::run<>({.title = "hello"},
          std::make_unique<theme_io::HotReloadTheme>("hello.theme"),
          std::make_unique<Hello>());
```

It polls mtime, writes ThemeDefaults (which `BeginUIContextManager` copies into
every context each frame, so a reload lands next frame), and seeds the file
from the live theme on first run so there is something to edit.

Two deliberate asymmetries: `load()` treats a file as a *partial override* of a
base theme, while hot reload reads onto a default `Theme` so the file is
authoritative and deleting a line restores that property's default. And a
malformed line costs one property, logged with its line number, rather than
failing the file — a typo must not silently swap in a whole default theme.

`tests/theme_io_test.cpp`, 62 checks. It caught a real bug while being written:
`primary = #112233  # note` kept the trailing comment, because the first `#` on
the line is the color. Comment stripping now runs per-side of the `=`.

Not covered: `language_fonts` and `font_sizing`. Fonts are asset wiring, and
`FontSizing` overloads its sign to mean "user-set vs interpolated", which does
not survive a naive round trip.

### 8b. Two defects found while verifying item 6

Both were caught only because hot reload gave a *behavioural* signal — the
theme visibly failing to change — that no compile or test would have produced.

**`ui::run()` never presented to the window.** `begin_frame`/`end_frame` in the
raylib windowed backend only bind and unbind an offscreen render texture;
blitting it to the window is the app's job (`begin_drawing` → draw the texture
→ `end_drawing`), as `puzzle`'s `present_render_texture` does. Without it the
window stays blank, and — because raylib ticks its frame timer and polls input
inside `end_drawing` — `GetFrameTime()` returns 0, the fps cap never applies
(measured ~640fps), and no input arrives. `run()` now presents. This shipped in
the item 8 commit; the frame captures that "verified" it read the offscreen
texture directly and so looked correct.

**Catalog dependency tracking was dead.** `-MD` names the dep file after the
output with its extension replaced, so `-o hello.exe` writes `hello.d`, while
`common.mk` included `hello.exe.d`. Every one of the 50 examples has been
compiling against stale binaries since `common.mk` was written — the same
class of silent-no-op as the earlier `-MMD` bug, and it is what made the
`run()` defect survive two rebuild-and-retest cycles. Fixed with an explicit
`-MF $(EXE).d`; verified by touching a header and confirming a recompile.

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

### 7. modal dialog sizing — **DONE**

Every dialog variant declared a height shorter than its own content. At 720p
the content is header 52 + message 96 + buttons 56 + padding 57.6 = 261.6, but
`create_*_content` asked for `h720(230)`. Since every piece scales linearly
(h720, and `Spacing::md` is `screen_pct(0.04)`), the shortfall was the same
~14% at every resolution — not a small-screen-only problem as first suspected.

It stayed hidden because `ImmTestHarness` registered no
`ProvidesCurrentResolution`, so `modal_impl` sized the box against a 1280x720
fallback while autolayout sized the children against the harness's real
800x600. The box came out too big relative to its content, which masked the
shortfall exactly.

Fixed: heights 230 -> 280 (and the taller variant 250 -> 300), and the harness
now registers the resolution it actually lays out at.

`check_dialog_layout` in dialog_test only checked the panel's *right* edge, so
nothing caught a row hanging out of the bottom. It now checks the bottom too,
and that check fails if the height is put back.

Also made the dialog fonts `h720` to match every box in the file. Mixing an
h720 box with a fixed-pixel font means text keeps its size while its box
shrinks. That was not what caused this overflow and no test covers it — it is a
consistency fix for the same class of bug.

### 8. `ui::run()` + `ui::default_keymap()` — the showcase itself — **DONE**

Items 1-5 shortened the *app*; what was left was the *harness*. Writing the
hello world made the remaining gap obvious — window init, input singletons,
system order, the frame loop, and shutdown came to ~15 lines of ceremony
around a 5-line app, and none of it varies between apps.

Two additions, both in `utilities.h`:

- **`ui::default_keymap<InputAction>()`** — conventional bindings for the UI's
  own actions. Entries match the caller's enum **by name** (via `magic_enum`),
  so an app with its own larger enum gets the widget/text bindings for free and
  keeps everything else; names the enum lacks are skipped. Reuses the existing
  `ProvidesInputMapping::GameMapping`, no new type.

- **`ui::run<InputAction>(graphics::Config, systems...)`** — opens the window,
  wires input + UI, runs the loop until close, shuts down, returns an exit
  code. Reuses `graphics::Config` (which already has title/width/height/fps)
  rather than inventing a config struct. Gated on a backend being defined, the
  same guard `window_manager.h` uses. Every piece it composes stays public.

The showcase, `examples/catalog/ui/hello/` — **17 lines**, 7 of them includes
and `using`s, against ratatui's 10:

```cpp
struct Hello : System<DefaultUIContext> {
  void for_each_with(Entity &entity, DefaultUIContext &ctx, float) override {
    if (button(ctx, mk(entity), "Hello World!"))
      printf("clicked\n");
  }
};

int main(int, char **) {
  return ui::run<>({.title = "hello"}, std::make_unique<Hello>());
}
```

That is a real window with a real event loop and a themed button. No
`InputAction` enum, no keymap, no singleton wiring, no system ordering, no loop.

**Verification limit:** the window, loop, theming and render are confirmed
visually (captured frame), and the keymap contents are unit-tested. What is
*not* verified end to end is that a keypress or click actually reaches the
widget through `run()` — that needs synthetic input against a live window,
which nothing here can do yet. The e2e `input_injector` is the obvious tool;
wiring it to a `run()`-driven window would close this.

It lives in the catalog rather than at `examples/hello/` as first written, so
it reuses `common.mk` and gets swept by the catalog build like everything else.

`tests/keymap_test.cpp` covers the keymap. The check that matters is
`every_default_action_is_bound`: an action with no binding is *silently* dead —
the widget systems query it every frame and it never fires, with nothing in the
build or at runtime to say so. That exact bug shipped in the consuming app,
where a menu input layer defined `WidgetNext` but not `WidgetMod`, so Shift+Tab
could not move focus backward. Mutation-tested by dropping the `WidgetMod`
binding: 4 failures, including the name printed as unbound.

Two things found while verifying, neither a regression:

- `run()` clears with `ThemeDefaults`, not `ctx.theme`.
  `BeginUIContextManager` (`systems.h:222`) overwrites the context's theme from
  `ThemeDefaults` every frame, so `ctx.theme` is a per-frame copy — clearing
  from it lags a frame behind any app that sets it.
- `Theme`'s default constructor (`theme.h:308`) fully overrides the member
  initializers at `theme.h:202-212`, so the documented defaults there
  (`background{45,45,55}` etc.) are dead code; the real default background is
  `oxford_blue`. Left alone, but the comments at those lines are misleading.

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

# Downstream app feedback

> **NOT derived from source TODO comments — do not delete during regeneration.**

Collected 2026-08-01 from the three apps that consume this library:

| App | Backend | Source docs |
|---|---|---|
| hanabi | sokol/Metal | `~/p/hanabi/afterhours_gaps.md` (#1-#18) |
| floatinghotel | sokol/Metal | `~/p/floatinghotel/docs/afterhours-gaps.md`, `afterhours-ui-footguns.md` (F1-F16), `afterhours_issues.md` |
| wm_afterhours | raylib | no gap doc; it is the reference consumer |
| puzzle | raylib | `~/p/armchair_coach/puzzle/design/docs/afterhours-focus-gaps.md` (D33-D38) |

Both gap-doc apps are on the **sokol** backend, so their reports skew there;
wm_afterhours exercises raylib. Of the gap-doc apps, only **puzzle** has
patched vendor (D33-D38, 2026-08-10); the others each have a shipped app-side
workaround instead, which is why none of it had surfaced as a bug report.

**wm_afterhours regression check (2026-08-01):** submodule fast-forwarded
`f06268e` → `5fe2e95` (26 commits, all of the ratatui-parity work). `make all`
builds clean and `make run-all-tests` is 7/7, exit 0. The one warning is a
pre-existing `-Wshadow` in its own `src/systems/screens/FighterMenu.h:270`.
Nothing broke.

## Already shipped — the docs are stale, tell the apps

Verified present in `src/`; the app docs predate them. No work here beyond
closing the loop so they can delete their workarounds.

- **`flex_grow(weight)`** — `layout_types.h:170`, an alias for `expand(weight)`.
  hanabi #18 asks for exactly this by name. See D2 for the caveat.
- **`FontWeight` / `with_font_weight`** — `component_config.h`, `color.h`,
  wired through `rendering.h`. Closes floatinghotel "No Font Weight Support".
- **`with_styled_label`** — `component_config.h`. Closes floatinghotel "No Rich
  Text / Multi-Color Text", which asked for this API almost verbatim.
- **`tree_view()` / `TreeNode<T>`** — `ui/tree_view.h`. Closes floatinghotel
  missing-primitive #3.
- **text_input clipboard** — `text_input/component.h:483-504` handles
  TextCopy/Cut/Paste/Undo/Redo. Closes floatinghotel "Clipboard shortcuts not
  wired".
- **text_input InputAction bootstrapping** — `default_keymap<InputAction>()`
  (todo item 8) binds every Text* action by name, so an app no longer has to
  hand-roll the enum values and mappings. Closes floatinghotel "text_input()
  requires InputAction enum values".
- **e2e `simulate_click` auto-release** — `auto_release` exists in both
  `e2e_testing/input_injector.h` and `test_input.h`. Closes the
  `afterhours_issues.md` "High" item.
- **ECS iterator UAF on `force_merge` during tick** — landed in `74afaf0`.
  Closes footgun F8.
- **Sokol headless rendering** — landed in `74afaf0` / `f06268e`. Closes
  footgun F9 (the *virtual resize* half too).
- **`disabled_opacity`** — in `theme.h`. floatinghotel's own doc already notes
  this; see D14 for the part that is genuinely still open.

## Open — ranked

Convergent items first: two independent apps hitting the same wall is the
strongest signal available, and both of the top two cost their app real
workaround code.

### D1. Sokol alpha blending — **DONE**, and now pixel-tested
Fixed: `metal_detail::g_blend_pip`, a src-over pipeline created once in
`setup_sokol_gl_and_fonts()` and loaded after both `sgl_defaults()` calls
(`backend.h:400` windowed, `drawing_helpers.h:1114` texture/headless).

Safe as a global default rather than opt-in: with straight alpha and `a == 255`
src-over is `src*1 + dst*0`, so opaque drawing is bit-identical. That is not
just an argument — the opaque case passes both with and without the fix.

**Proven, not asserted.** `sokol_blend_test` renders headless into an offscreen
texture and reads the pixels back, so it checks what the GPU produced rather
than which calls were issued. Without the fix, half-alpha red over blue comes
back `rgba(255, 0, 0, 255)` — and so does a fully *transparent* draw. That is
the reported symptom exactly: the alpha byte reaches `sgl_c4b` and is discarded.

**Text was never at risk**, which was the one thing worth checking before
changing a global pipeline. `sokol_fontstash.h:2401-2408` wraps its draw in
`sgl_push_pipeline()`/`sgl_pop_pipeline()` around its own alpha-blended
pipeline, so text has always blended and is fully isolated from the sgl default.
Shapes never blended; text always did.

This also closes the first sokol test target. D19 (Metal headless hi-DPI) and
D20 (`load_texture` mipmaps) are now testable the same way.

### D1 (original report, kept for context)
**hanabi #13 + #15, floatinghotel "Div backgrounds render opaque".** Root
cause is one line: `begin_drawing` calls `sgl_defaults()` every frame, which
loads the sokol_gl default pipeline, and sokol_gfx defaults blending to *off*.
Everything downstream inherits it:
- `draw_texture_pro` blits a transparent PNG's `a=0` texels as **opaque black**
  (hanabi #13) — so an icon atlas is unusable as authored.
- `with_custom_background(Color{r,g,b,31})` fills a **fully saturated opaque**
  quad; the alpha byte reaches `sgl_c4b` and is discarded (hanabi #15).
  Pixel-sampled: token said `a=31`, screen showed `a=255`.
- `with_opacity(0.32f)` on an overlay div likewise renders opaque
  (floatinghotel), so a translucent selection wash is impossible.

Workarounds shipped: hanabi builds its own blend-enabled `sgl_pipeline` and
push/pops it around every blit; hanabi *also* added `theme::over(fg, bg)` to
pre-composite tints against a **known** backdrop colour — brittle the moment
surfaces stack. floatinghotel draws an opaque selection box then re-draws the
selected substring on top of it.

Fix: enable src-over blending on the sokol_gl pipeline used for 2D UI. Raylib
backend is unaffected, so this needs a sokol-side test. Translucent tint
surfaces (chips, hover overlays, selection washes, badges) are a core idiom —
this is the single highest-value item in the list.

### D2. Row-flex `expand()` — get a repro or close it
**hanabi #18, floatinghotel "Row Flex Layout Broken with expand() Children" +
footgun F5.** These two reports do not agree, and the evidence points at
discoverability rather than a layout bug:
- `flex_grow(w)` / `expand(w)` exists (`layout_types.h:170`).
- `autolayout_test` is **332/332 green** today, including
  `expand_fills_remaining_row`, `multiple_expand_share_space`, and
  `expand_with_padding`. (Note: the 2 failures recorded in the older makefile
  plan are gone.)
- hanabi #18 only ever describes trying `percent(1.0f)` and a fixed
  `percent(0.72f)`, then asks for "`with_flex_grow(int)`" — i.e. it never found
  the function that already does this.
- floatinghotel F5 cites "a known Row-flex expand() bug (referenced in the
  repo's own docs/afterhours-gaps.md)" — which is its own doc. Circular.

But floatinghotel's gaps doc makes a *specific* claim the passing tests do not
cover: a Row-flex **`button`/`div` with children**, where an `expand()` child
takes 100% and the fixed sibling wraps below. Both apps paid for this: hanabi
hand-computes `labelW = rowContentW − leadSlot − countColW` across three
different row types; floatinghotel bakes whole rows into one label string.

**Resolved: not a bug.** Built the repro seven ways and every one passes.

Engine level (`autolayout_test`, 344/344): the reported row with the `NoWrap`
the old test set, *and* with the `FlexWrap::Wrap` default that app code
actually gets, under both a fixed-pixel and a `percent(1.0f)` parent.

imm level (`downstream_gaps_test`, the layer the apps call): `div()` rows,
a `percent`-sized row, `button()` with children, and the assertion hanabi
actually wanted — two rows of *different* leading-slot width landing their
trailing counts on one shared right edge, which is the ~17px gap it could not
close.

Two things worth passing back:
- hanabi asked for "`with_flex_grow(int)`". That is `expand(weight)`, aliased
  as `flex_grow(weight)` since `layout_types.h:170`. It never found the
  function, and `percent(1.0f)` — the thing it did try — correctly means "all
  of the parent", not "the remainder". Pure discoverability.
- The `button()` case is the trap worth documenting. Unlike `div()`, `button()`
  carries default padding, so a row of width 300 with a 16px sibling gives the
  expanding child **252**, not 284. That reads exactly like "expand is wrong"
  if you measure against the outer rect instead of the content box. Betting
  this is what floatinghotel actually saw.

Follow-up: say so in the README sizing table (`expand` vs `percent`, and that
`button()` pads), and tell both apps they can delete the pixel bookkeeping.

### D3. No hit-test priority or click consumption for overlapping widgets
**hanabi #3, floatinghotel F13b.** Two shapes of the same gap:
- Absolutely-positioned siblings that overlap (a tab body and its close ×)
  return ambiguous `ElementResult`s (hanabi).
- A `button()` nested inside a row that has its own `HasClickListener` **never**
  fires — the row's listener always wins, and there is no `stopPropagation`
  (floatinghotel). Render order and child-ness do not change priority.

Both apps fell back to manual `is_mouse_inside(ctx.mouse.pos, rect)` +
consuming `ctx.mouse.just_pressed`. floatinghotel's positional fallback *also*
failed, on D10's coordinate-space mismatch — so it shipped the bug (a Refs
checkout click with no row to land on). "A control inside a clickable row" is
not achievable today.

Ask: topmost-wins hit priority by render layer / child depth, plus a way for a
handler to consume a click.

**DONE** — topmost-wins, no bubbling. `ResolveHitTarget` picks one winner per
frame (highest `render_layer`, ties to later in paint order) and points *both*
`hot` and `active` at it; `HandleClicks`/`HandleDrags` stopped resolving inline.
Nothing else was needed: `is_mouse_press` already requires `is_hot && is_active`,
so once they name one element only that element activates. Same rule ImGui uses
— `ActiveId` is only granted to an item already hovered.

Root cause was not precedence. The two fields disagreed: `set_hot` is
last-writer-wins, `set_active` is first-writer-wins, and a press needs both. A
nested child ended up hot but never active, so it fired nothing while its
parent, visited first, fired. Making `set_active` last-wins does not fix it —
the parent has already decided mid-iteration and both would fire.

Also deleted `HandleClicks::process_derived_children`: the system already
visits every entity with the component, so it processed children twice.

Not done: bubbling / `stopPropagation` (needs an event object in every
callback, nothing needs it yet) and an input-passthrough flag (an overlay with
no click listener is already not a candidate).

Worth knowing: the failure is **iteration-order dependent**, so it does not bite
everywhere. wm's menus worked before the fix by ordering luck — verified by
reverting and re-running. hanabi's tab bar is on the unlucky side: tab
`mk(uiRoot, 910+i)` at `render_layer(baseLayer)` and its close × at
`mk(uiRoot, 950+i)`, `baseLayer + 1`, overlapping siblings where the lower id
wins. That is the shape `hit_priority_test` covers.

### D4. `FlexWrap` defaults to `Wrap` — **DONE**, now `NoWrap`
Flipped in both `component_config.h` and `ui_core_components.h`. Three things
had to move with it, none of them obvious from the report:

- **There was no `with_wrap()`**, only `with_no_wrap()`. Flipping without adding
  it would have made wrapping unreachable from `ComponentConfig`.
- **`apply_overrides` used `FlexWrap::Wrap` as its "caller did not set this"
  sentinel**, so it inverts when the default moves. (Every field in that
  function has the same default-as-sentinel property: you can never override
  *back to* the default. Pre-existing, just relocated.)
- **Three imm widgets did `if (flex_wrap == Wrap) with_no_wrap()`** to mean
  "caller did not choose". After the flip `Wrap` only appears when the caller
  asked for it, so those would have overridden an explicit request. Removed.

Also removed a wrap-warning condition that keyed off `child.flex_wrap`. It read
the wrong field — the *parent's* flex_wrap decides whether a child wraps — and
once `NoWrap` was the default it fired on every child of a deliberately
wrapping parent. Condition 1 (parent NoWrap + overflow) covers the real case.

One warning became newly visible rather than newly wrong: cross-axis overflow
was suppressed when the parent wrapped (`autolayout.h:1452`), so a
`percent(1.0f)` child plus margin now warns under the content-box model. That
is accurate, and the test that documents the overflow as intentional declares
it.

**Correction to an earlier claim in this file.** I first recorded that the flip
changed nothing in wm_afterhours. That measurement was invalid: `rsync -a`
preserves source mtimes, they landed in the same second as the existing object
files, and make skipped the rebuild — the numbers came from a stale binary.
Always confirm the binary is newer than the sources before trusting a result
here; this repo has produced sub-second mtime races three times.

**What clean builds actually show** (same commit, D4 reverted vs not):

| | overflow warns | wrap warns | flex_alignment |
|---|---|---|---|
| without D4 | 28 | 24 | 1.3079% |
| with D4 | 128 | 34 | 1.7796% |
| with D4 + margin fix | 54 | 34 | 1.7796% |

The 128 was a bad diagnostic, not bad app code: 98% of those overflowed by
≤32px, spread across ~50 distinct widgets — one idiom, `percent(1.0f)` plus a
margin, which the content-box model *guarantees* will overflow. Warning on it
told developers to stop doing something the library documents (there is a test
enshrining it as "by design"). The overflow check now allows overflow within
the child's own margin.

The residual ~36 are all on one screen and are **genuine**: three fixed 26px
boxes ending at x=78 inside a 49.8px-wide container. They used to wrap out of
sight. That is exactly what D4 is for, and it is why that screenshot moved —
the app can add `.with_wrap()` if wrapping was the intent.

### D4 (original report, kept for context)
**floatinghotel F4b, called "the single nastiest default we hit".** Confirmed
still `FlexWrap::Wrap` at `component_config.h:62` and
`ui_core_components.h:93`. Any Column taller than its viewport silently wraps
its children into a **second column off-screen** instead of scrolling — you see
stray text fragments hugging the right edge. Every scroll container and every
stacking Column needs an explicit `.with_no_wrap()`.

Ask: default `NoWrap`, or at minimum default it for `Overflow::Scroll/Auto`
containers, and document that the current default wraps. A breaking change, so
it wants a deprecation cycle — but four apps each rediscovering this is worse.

### D5. Unmapped child ids segfault the layout pass — **DONE**
**floatinghotel `afterhours_issues.md`, "Critical".** Fixed; repro'd first.

The report blamed `to_ent` (`autolayout.h:83-91`), which bounds-checks,
`log_error`s, then returns `*mapping[id]` anyway — the log line is immediately
followed by the segfault it just described. That is real, but it was **not**
the reachable crash. Two corrections from building the repro:

- The call site floatinghotel named — the `children()` text-measurement
  fallback from `c10c0aa` — has since been guarded (`autolayout.h:652-659`).
  Their doc predates the guard.
- The live crash was one level lower and much broader. `cmp(id)`
  (`autolayout.h:67`) is an *unchecked* `*cmp_cache_[id]` with no bounds test at
  all, and it is what every tree walk uses. ASan put the fault in
  `reset_and_calculate_standalone` (`autolayout.h:1544-1545`) — the very first
  traversal, before any sizing runs. So the crash needs no text sizing, no
  labels, and no `children()`: any stale child id is enough.

Trigger is exactly what `UIEntityMappingCache` already documents at
`systems.h:152-154` — the mapping is rebuilt each frame from live UI entities,
so a child cleaned up while its parent still lists it leaves a null hole.

**Fix:** `prune_stale_children()` walks the tree once after `build_cmp_cache()`
and drops unreachable child ids, so `cmp()` stays an unchecked deref on the hot
path and every present *and future* traversal is safe by construction. Guarding
the ten-odd `for (EntityID child : widget.children)` loops individually would
have been a bigger diff that the next such loop would silently opt out of.

**Tests** (`autolayout_test.cpp`, 4 added, 341/341 green): `stale_child_id_is_dropped`
(exits 139 without the fix), `stale_subtree_is_dropped_whole`,
`stale_child_with_text_sizing_is_dropped`, and `prune_keeps_every_live_child` —
the last one guards against a bad `has_cmp()` bounds check quietly eating live
ids, which is the way this fix would fail.

**Still open:** `to_ent` and `UIEntityMappingCache::to_ent` (`systems.h:160-166`)
both still deref after logging. Pruning makes them unreachable from the layout
walk, but they remain traps for any new caller. Worth converting to a
pointer/optional return separately — it is a mechanical change across ~6 call
sites, each of which has an obvious unmapped fallback (a debug name, `false`
for `has<HasScrollView>()`, `0` for a text size).

### D6. `with_text_overflow(Ellipsis)` hangs with `expand()` / `children()` sizing
**floatinghotel gaps doc.** Infinite hang **on launch**, not a wrong pixel. The
renderer's binary search for the longest fitting prefix runs against a 0-width
container before layout resolves and never terminates. Only safe on fixed-pixel
widths today. Also crash class; pairs naturally with D5.

**Resolved: no hang, and truncation works.** Now covered end to end by
`d6_ellipsis_with_expand_terminates_and_truncates` and
`d6_ellipsis_with_children_sizing_terminates`, which run the real renderer via
the D6b harness: the long label comes back shortened and ending in `...`, from
an `expand()`-sized element, in finite time.

Getting there took two passes, both worth remembering. The first version was
vacuous because the harness never rendered (D6b). The second still was, more
subtly: the `none` backend's `measure_text` returned `{0,0}`, so
`text_size.x <= max_width` was trivially true and the truncation code returned
early at `rendering.h:707` without ever reaching the binary search. Both
versions were green. Only installing a real measure function made the two
truncation assertions fail-then-pass, which is the only evidence that they
test anything.

Original analysis holds: the search at `rendering.h:722-737` terminates, and
the zero-width theory in the report cannot reach the loop because `max_width`
goes negative and returns at `:702`.

**Kept below for context.**

The *layout* half does not hang. `d24_tab_container_respects_parent_origin` and
friends exercise exactly the suspect combination — `tab_container` sets
`TextOverflow::Ellipsis`, sizes tabs with `expand()`, and pins
`set_min_width(Size{Dim::Text, ...})` (`imm_components.h:1874-1886`) — and they
complete.

The *render* half reads as sound too. The binary search
(`rendering.h:722-737`) terminates: `low` only rises, `high` only falls,
`mid == 0` breaks before the `high = mid - 1` underflow, and a zero-width rect
returns early at `:702` (`max_width = rect.width - 10.f` goes negative, so the
0-width-container theory in the report cannot reach the loop). The guards look
like someone already fixed this.

Cannot fully close it, because the render path is untestable here — see D6b.
If floatinghotel still hangs on current afterhours, ask for a stack sample; a
hang is one of the easiest things to get a definitive answer on.

### D6b. The imm test harness never renders — `rendering.h` has zero coverage
Not an app report; found while trying to test D6. `ImmTestHarness::layout_and_render()`
(`tests/ui_test_harness.h:164-181`) runs `AutoLayout::autolayout()` and then
sets `was_rendered_to_screen = true` on every component. It never calls the
renderer. The name says otherwise, and I wrote a green D6 test against it
before noticing — a test that never executed the code it was testing, which is
worse than no test.

Consequence: everything in `rendering.h` is untested — ellipsis truncation,
colour resolution, opacity, per-side borders, `on_draw_fg`. That is also where
several open items live (D1 blending, D6 truncation, D14 disabled dimming), so
it blocks more than it looks.

**DONE.** Three parts:

1. Renamed to `layout_only()` across all 66 call sites, so it stops implying
   coverage it does not provide.
2. The `none` backend now **records** draw calls instead of `log_error`-ing
   them (`backends/none/drawing_helpers.h`). It exists so the library builds
   with no GPU; logging made it useless for testing. Recording costs nothing
   and needs no new abstraction — the backend switch in `drawing_helpers.h`
   was already the seam. Only the ops the UI render path emits are recorded;
   the rest still log, because an unimplemented call reached by accident
   should stay loud.
3. `ImmTestHarness::render()` drives `RenderImm` for real and returns the
   recorded calls; `drawn(op)` filters them.

`AFTER_HOURS_BACKEND_NONE` is defined by `drawing_helpers.h` when it falls
back, so `render()` is *compiled out* under a real backend rather than offered
and silently recording nothing. Render-asserting suites must stay out of
`RAYLIB_TESTS`; `downstream_gaps_test` was moved out.

**The bit that mattered most:** `none`'s `measure_text` returned `{0,0}` while
its own warning told callers to "provide your own through
`set_measure_text_fn()`" — a setter that did not exist for this backend.
`AutoLayout` has such a hook, but the renderer calls the free function
directly. So every measured-width branch in `rendering.h` took the it-fits
path, and a truncation test passed while testing nothing. Added the seam to
`backends/none/font_helper.h` and installed the harness's existing layout stub
through it, so layout and render finally agree on text width.

Verified: 232 tests across 18 suites green, catalog 50/50.

Now unblocked: D14 (disabled dimming) is directly testable. D1 is not — it is a
pipeline-state bug, invisible at the draw-call level, and still needs a real
sokol target.

### D7. Nested scroll containers stop rendering children
**floatinghotel F7b + F13.** A `ScrollPanel` nested inside another scroll
container silently stops rendering its rows once the UI has been exercised —
hit twice (sidebar file list, Refs branch list), and the branch case **shipped
as a bug**. Suspected same root as F13: `HasScrollView` only enables scroll
when `content_size.y > viewport_size.y`, and `content_size` is summed in
`FixScrollViewPositions` from resolved child heights — if heights don't resolve
on a frame, scroll silently disables and the panel reports empty.

Workaround both times: render rows into the *outer* panel. Ask: make nesting
work, or at minimum warn when a container resolves to zero content while it has
children. Related trap from F4b: a nested container sized `children()` inside a
fixed-height scroll parent gets clamped to the remaining viewport, so siblings
below it overlap.

### D8. Labels don't word-wrap
**floatinghotel F1.** `with_word_wrap(true)` sets `text_area_word_wrap`
(`component_config.h:392`) which only the multiline *text-area* widget reads. A
long single-line label — a commit-message body — just overflows off-screen.
There is no wrapping mode for plain `div`/label text bounded by its
`percent()`/`pixels()` width.

### D9. Font atlas is ASCII-only (Basic Latin)
**floatinghotel F2.** Every non-ASCII glyph renders **blank**: `←`, `▾`, `✓`,
box-drawing, icon glyphs. floatinghotel fell back to `<- Back` and "More"
instead of "More ▾", and text buttons instead of icon buttons — an icon-heavy
mock cannot be matched without shipping an icon font. Ask: document the atlas
coverage prominently, and support supplemental glyph ranges or an icon font.

### D10. One coordinate space, please
**floatinghotel F4b(a) + F13b.** Three spaces disagree today:
- `measure_text` returns **physical** px on a HiDPI/headless framebuffer while
  layout rects are logical — so a `measure_text`-driven wrap wraps at half
  width.
- e2e `click x y` injection space and `get_mouse_position()` return space don't
  agree, which is what defeated floatinghotel's positional workaround for D3.

Ask: one consistent space across `get_mouse_position()`, e2e click injection,
layout rects, and `measure_text` — or at least a documented conversion.

### D11. text_input ignores `with_font_size`/`with_custom_background` — **DONE**
The field derived its font size from its computed height and forced its fill to
`Theme::Usage::Secondary`, dropping both config calls. The height-derived size
is now a *fallback* used only when `font_size_explicitly_set` is false, and the
theme fill is only imposed when the caller did not set a custom background.

Two things the tests surfaced:
- `text_input` sizes from the PREVIOUS frame's height, so a single emit+layout
  never reaches that code at all. The tests run two frames like a real app;
  without that the control case reads the ComponentConfig default (50) instead
  of the derived 40 and proves nothing.
- They cannot live in `downstream_gaps_test`: `text_input` pulls in
  `graphics.h`, which requires a real backend macro. So `text_input_test` is a
  raylib suite asserting on component state after layout.

Also found: `text_input` references `TextSelectAll`, `TextBackspace`,
`TextDelete` and `MenuBack` on the InputAction enum *without* the `if constexpr`
guard the clipboard actions use, so the harness enum had to grow them. That is
floatinghotel's "text_input requires InputAction enum values" only half-fixed;
guarding the rest would let the widget degrade instead of failing to compile.

### D11 (original report, kept for context)
**hanabi #17.** The widget *derives* its font size from computed field height
(`text_input/component.h:187`, `derived_fs = field_h * 0.5f`) and forces the
inner field fill to `Theme::Usage::Secondary` (`:163`). Both config calls are
silently ignored: a 72px field rendered hanabi's draft at ~36px and overflowed,
and the field stayed dark even in the app's Light theme. Workaround: pin the
field to ~34px so the derived font lands readable, and accept the wrong colour.

Ask: honour an explicit `with_font_size` when set, falling back to the derived
size only when unset; let `with_custom_background` override the forced fill.

### D12. text_input has no `with_placeholder` — **DONE**
`ComponentConfig::with_placeholder(std::string)`. The hint shows while the
bound string is empty and draws in `theme.font_muted`; it survives focus and
clears on the first character, matching the platform convention.

It routes into the field's `HasLabel` only, never into `display_text`, because
the cursor and horizontal-scroll maths read that — sending the hint through it
would park the caret after the hint instead of where typing starts. A test
focuses the field and compares the caret x with and without a long placeholder.

The muted colour is cleared explicitly on non-placeholder frames: imm entities
persist, so setting a colour without unsetting it leaves real text muted once
the field has been empty.

Unblocked by D11 — the forced `Secondary` fill was what made hint text painted
*behind* the field invisible, which is why the reporting app used an
absolutely-positioned overlay with hand-derived geometry instead.
**hanabi #17 follow-on (2026-08-01).** Grep-confirmed absent. An empty field is
a bare box — hanabi's sidebar search read as an unlabelled box + magnifier and
was written up as a hostile-review defect. Compounded by D11: because the field
force-fills opaque `Secondary`, a placeholder painted *behind* the input is
covered by the field's own fill, so the obvious workaround doesn't work either.
hanabi ships an absolutely-positioned overlay child at a higher render layer,
with its origin hand-derived from panel geometry (panel xy → header height →
search-wrap/field paddings + magnifier slot). Fix D11's forced fill and this
becomes a ~10-line addition.

### D13. Missing container/menu primitives — **DONE** (branch `d13-anchored-overlays`)
`overlay::place()`: anchored placement with edge flipping, the piece all three
widgets need and all three hand-rolled downstream. Pure geometry, 16 checks.

Wired into `dropdown`: an open tray near the bottom now flips above the trigger
instead of running off screen. Two tests cover both directions.

Built on top: `MenuItem` (label, shortcut, separator, disabled), `menu_list`,
`dropdown_menu`, `context_menu`, `popover`. 53 checks in `menu_test`, plus the
`menu_showcase` screen and screenshot baseline in wm_afterhours.

Dismissal is focus-based rather than a generalised `ModalCloseWatcherSystem`.
`dropdown` already closes on focus loss (`imm_components.h:1677`) and that one
check covers click-outside and tab-away without any hit testing, so the menus
reuse it instead of growing a second mechanism. Consequence: only one menu can
hold focus, so opening a second closes the first.

`popover` needs `detail::focus_within` rather than exact-id focus — a menu
closing when an item takes focus is correct, a popover doing it when you click
its own text input is not. It walks UP the parent chain, because the tree is
cleared each frame and a popover runs its check before the caller re-adds
content; walking down always sees an empty subtree.

Not done: Escape-to-close. `dropdown` does it via an unguarded `IA::MenuBack`,
which fails to compile for apps whose `InputAction` lacks that value — the same
portability problem as D23's `text_input` note. Wants a `magic_enum` guard.

### D13b. NOT a library bug — the harness was missing the per-frame reset
Kept because two wrong theories got written down before the measurement was
right, and the sequence is the useful part.

Wiring `overlay::place()` into `dropdown` produced absurd positions. First
theory: an absolute child double-counts its parent's offset — disproven by a
minimal repro returning the sensible parent-relative answer. Second theory:
absolute positions accumulate across frames — they did, `computed_rel` grew
1000 → 2680 → 5480 → 9400, but that was a symptom.

Actual cause: `ImmTestHarness` never ran the equivalent of
`ClearUIComponentChildren` (`systems.h:288`), which real apps run before
emitting each frame. Without it a reused imm entity is appended to its parent
again every frame, so `compute_rect_bounds` adds the parent offset once per
duplicate. The library was correct throughout; the harness was lying.

`ImmTestHarness::begin_frame()` now clears the tree and multi-frame tests call
it. Any new multi-frame test must too. A single emit+layout hides this
entirely, which is why it went unnoticed — every earlier test happened to be
single-frame.

### D14b. Disabled text and disabled backgrounds use different transforms
Surfaced by the de-duplication above, not by an app. Disabled **text** darkens
by `colors::darken(c, 0.5f)`; a disabled **background** goes through
`Theme::disabled_variant` (mix toward background + alpha + desaturate). Two
transforms for one state, and only the background one honours
`Theme::disabled_opacity` — so an app tuning that knob moves its backgrounds
and not its text.

Left alone deliberately: unifying them changes how disabled text looks in every
existing app, which is a visual decision worth making on its own rather than
smuggling into a bug fix. `d14_disabled_text_keeps_its_own_darken_transform`
pins today's behaviour, so whoever unifies has to come update it.

### D15. Roundness is a fraction, not pixels
**floatinghotel F7.** `with_roundness(f)` is a fraction of half the min
dimension, so "an 8px radius card" has to be back-solved per element size, and
the same fraction looks wrong on a tall card vs a short button. Ask: a
pixel-radius option alongside the fraction.

### D16. `percent()` doesn't resolve reliably in nested containers
**floatinghotel F6.** The sidebar threads an explicit `pixels(sidebarPixelWidth_)`
down to every child section because `percent()` resolves wrong inside nested
divs. Note `c10c0aa` fixed the *absolute-positioned-parent* case specifically;
this is the plain-nesting case and may be the same bug or a different one — no
repro captured. Wants one before it can be actioned.

### D17. No measured-text size unit
**floatinghotel F3.** Menu-header widths come from a hardcoded `charW ≈ 10px`
estimate that had to be re-tuned by hand whenever font tiers changed.
`measure_text` exists but isn't reachable from layout sizing. Ask: a `Size`
mode that resolves to measured text width — see D10, it must resolve in
*logical* px.

### D18. Mouse-wheel injection is consume-once and order-sensitive
**floatinghotel F12.** e2e `scroll_wheel` sets a wheel value that
`get_mouse_wheel_move_v()` *consumes* on first read, cleared per frame — so
whichever system reads first wins and targeting a specific scroll view
headlessly is unreliable. The real app reads a live, re-readable wheel, so
**headless behaviour diverges from real**, which is the part that matters for a
test harness. Ask: non-consuming reads in test mode, or per-target routing.

### D19. Metal headless capture can't supersample — **DONE**
`Config.hidpi` is now honoured by the sokol headless path: the offscreen target
is allocated at `width*scale x height*scale` (`hidpi_scale`, default 2),
`render_scale()` is set, and the ortho projection stays in LOGICAL pixels so
the same drawing rasterises into more pixels instead of shrinking into a
corner. `RenderTextureType` carries a single `scale` and derives logical size
as `width / scale`; scale is uniform everywhere already (raylib reads
`GetWindowScaleDPI().x` and drops `.y`, sokol's `sapp_dpi_scale()` is one
float, `render_scale()` is one int), so two logical fields were redundant and
could disagree. `render_scale()` moved out of the raylib-only branch.

Tested by capturing at 2x and sampling: the buffer is `(w*2)*(h*2)*4`, and a
rect covering the logical left half is green across the left half of the 2x
image. Three checks fail with the scale forced to 1.

### D20 (original report, kept for context) — **DONE**
sokol has no runtime mipmap generation — levels must be supplied at image
creation — so `load_texture` now builds a box-filtered chain on the CPU and
uploads every level. `gen_texture_mipmaps` becomes a no-op (images are
immutable and the chain already exists) instead of logging @notimplemented.

Costs ~33% more texture memory, the standard trade. Tested on the builder
directly since it is deterministic: a solid image keeps its colour to 1x1, a
black/white 2x2 averages to exactly 128, and odd non-square sizes still
terminate at 1x1.

### D19 (original report, kept for context)
**hanabi #6.** The windowed Metal path sets `desc.high_dpi = true`, so the live
app is crisp. The *headless* path (`DisplayMode::Headless` → `metal_init`)
creates a fixed `cfg.width × cfg.height` offscreen texture at 1x.
`graphics::Config.hidpi` is read **only** by the raylib backend
(`backends/raylib/windowed.h`); sokol/Metal ignores it and never sets
`graphics::render_scale()`. Rendering into a 2x texture is *not* a workaround —
adaptive UI just lays out at the larger logical size. Consequence: hanabi's
`--screenshot` PNGs, used for docs and pixel-perfect phase validation, are soft
and under-represent the real window. Ask: honour `Config.hidpi` in the Metal
headless path — allocate at `width*scale × height*scale`, set
`render_scale(scale)`, keep the ortho projection in logical px (the same
contract windowed high_dpi already uses).

### D20. `load_texture` generates no mipmaps
**hanabi #14.** Sokol backend creates its image with
`mipmap_filter = SG_FILTER_NEAREST` and a single mip level. A texture drawn
much smaller than source minifies off the full-res level with no pyramid, so
thin high-contrast features (line-icon strokes) alias and shimmer. No app-side
way to request mipmaps, trilinear, or anisotropy. hanabi's workaround is to
author the atlas near the real draw size (regenerated at 32px cells, was 64px)
to keep minification under ~2x where plain bilinear is clean. Ask: generate a
mip chain in `load_texture` with `mipmap_filter = LINEAR`, or a variant that
opts in.

### D21. No OS appearance (light/dark) query
**hanabi #1 and #16 — filed twice, so it kept mattering.** Nothing exposes the
host appearance (macOS `AppleInterfaceStyle` / `NSApp.effectiveAppearance`) or
an appearance-changed hook. hanabi's settings panel offers Light / Dark /
System; Light and Dark work, **System cannot be honoured** and silently renders
Dark with a footnote apologising for it. "Match system" is table stakes for a
native desktop app. Ask: `graphics::os_appearance() -> {Light,Dark,Unknown}`
plus a changed callback. hanabi notes an app-side ObjC++ shim is viable, so
this is a convenience — but every app will otherwise write the same probe.

### D22. Animation gaps (post-MVP)
**hanabi #8-#12.** The `Anim` builder (`ui/animation_config.h`) plus the
key-based manager (`plugins/animation.h`) already cover hover, press-spring,
appear fade, loop pulse, idle float, and count tick. Residual, all non-blocking:
- **#9 OnExit / leaving lifecycle** — hanabi calls this "the single biggest
  structural gap". Triggers are `OnAppear/OnClick/OnHover/OnFocus/Loop`
  (`animation_config.h:14-20`); in immediate mode a departing widget simply
  isn't emitted next frame, so nothing can animate out. Wants a "keep alive M
  ms after last emit and run its exit anim" hook. Genuinely hard in pure
  immediate mode — the interesting design problem in this list.
- **#8 stagger/delay** — `AnimationDef` (`animation_config.h:47-60`) has no
  `delay`/`stagger_index`, so a list fades in all at once with no cascade.
  Smallest of the five: one field applied before the track goes active.
- **#10 OnValueChanged** — no "underlying value changed" trigger for a one-shot
  flash when a row's status flips.
- **#11 shimmer sweep** — needs a linear-gradient fill/mask primitive; a pulsing
  skeleton is already trivial via `loop().opacity(...)`.
- **#12 drag gesture + spring-to-slot** — no pointer-delta model, so spring-based
  tab reorder isn't expressible. Note this compounds D3.

### D23. Smaller ergonomics
- **Tween/AnimatedValue helper** (hanabi #2) — every animated property is
  hand-rolled `animT += dt/dur` + `smoothstep`. hanabi calls this "arguably not
  a real gap, just boilerplate". Low priority.
- **Synchronized scroll views** (floatinghotel) — side-by-side diff; today you
  mirror the offset manually each frame.
- **Virtualized list** (floatinghotel) — 10k+ commit logs; today you hand-roll
  windowed rendering inside `scroll_view()`.
- **`with_font_tier()` only supports `h720()` scaling** (floatinghotel) —
  adopting tiers forces proportional font scaling; no tier + fixed `pixels()`
  path.
- **No card preset** (F15) — `with_border`/`with_border_bottom` are good, but
  every card re-specifies bg + border + radius + padding.
- **Absolute children need manual `with_render_layer`** (F16) to stack
  correctly (graph dots over lines); easy to forget.
- **Raw font sizes drift** (F14) — call sites bypass the `FontSize` tier enum
  with raw `h720(px)`/`pixels(px)` and nothing discourages it. A lint or a
  "no raw font sizes" mode would hold the scale.
- **`TextOverflow` under-documented** (F4) — default is `Clip` and the
  interaction with `NoWrap`/flex is easy to get wrong.
- **`window_manager` forward-declares `sapp_*`** (F11) to stay decoupled, so the
  headless resize path couldn't reach `graphics::` without adding the include.
  Minor; the decoupling fought them.
- **Offscreen readback needs manual GPU sync** (F10) — non-MSAA Private Metal
  render targets return garbage from `getBytes`; you must blit to Shared +
  `waitUntilCompleted`. Not a code gap, a documentation one.

### D24. Known-broken widgets floatinghotel routes around — **NOT REPRODUCIBLE**
- **`tab_container()` renders at screen-absolute position**, ignoring parent
  bounds — unusable for multi-repo tabs; they build manual tab buttons in a row.
- **`toggle_switch()` creates sibling entities that consume layout space**, so
  adjacent elements misalign; workaround is `with_no_wrap()` on the parent plus
  a taller container.

Neither reproduces. Both now have coverage in `downstream_gaps_test`, which
they did not before — the two pre-existing `tab_container` tests are about
label widths, and `toggle_switch` had no tests at all, which is presumably how
these shipped broken in the first place if they ever were.

Four tests, covering the stated conditions *and* the likeliest underlying
cause:
- `d24_tab_container_respects_parent_origin` — bar nested in an offset panel
  lands at the panel's origin, not the screen's.
- `d24_tab_container_under_absolute_parent` — the same bar under an
  absolutely-positioned ancestor. This is almost certainly the original bug:
  `c10c0aa` fixed "percent(1.0f) resolved to screen width inside
  absolute-positioned parents", and a tab bar defaults to `percent(1.0f)`
  width, so the report is that bug wearing a `tab_container` costume.
- `d24_toggle_switch_stays_within_its_row` — the row below is not pushed down.
- `d24_toggle_switch_in_narrow_parent_does_not_wrap` — parent squeezed below
  label + 52px track, the condition their `with_no_wrap()`-plus-taller-container
  workaround implies.

Caveat: not-reproducible is weaker than fixed. These cover the conditions the
reports describe, not necessarily the app state that produced them. If either
app still sees it on current afterhours, the next thing to ask for is the
containing hierarchy, since both suspicions point at the ancestor chain rather
than the widget.

## Out of scope for this library

Recorded so they aren't re-litigated:

- **macOS menu-bar extra / NSStatusItem** (hanabi #5). afterhours only creates a
  normal window; hanabi will do NSStatusItem + NSMenu in its own `.mm`. The only
  plausible upstream ask is a hook to run app code without owning the main
  window. Deferred by hanabi to its Phase 4.
- **GPU memory knobs** (hanabi #7). Placeholder against a <250MB RSS target;
  baseline is ~70MB windowed, so nothing is measured yet. If it ever bites, the
  ask would be configurable font-atlas dimensions (fixed 2048x2048 today) and a
  texture-cache eviction hook.
- **Status-glyph shapes** (hanabi #4) — logged then self-resolved. `draw_triangle`,
  `draw_poly`, `draw_circle_v` plus `with_on_draw_fg` already do it. Kept only as
  evidence that `with_on_draw_fg` is under-advertised.

---

# Gap sweep 2026-08-03 (post-D3)

Re-read the four app gap docs. hanabi's was updated **the same day** (a full
text-input requirements spec); floatinghotel's and wm's were 3 and 6 days old.

**Closed by today's work, no action needed:**
floatinghotel's "Missing Primitives" 4/5/6 — `dropdown_menu`, `context_menu`,
`popover` — are D13, shipped. Its 1/2/3 stay open and are the only gaps in any
doc still labelled **BLOCKER**.

### D25. No subtree-hover query, and no hit-test-ignore flag
**hanabi #29.** One global `hot_id`, so a hoverable child steals its parent
row's hover fill: hanabi's sidebar thread row FLICKERS its whole-row wash as the
pointer crosses onto the trailing star toggle. Two things are missing — a cheap
"is the mouse anywhere in this subtree?" query (the tree hit-test helper is
internal and scroll-offset-aware), and any hit-test-exclusion flag
(`with_skip_tabbing` is focus/tab only, it does not affect hot).

The workaround is expensive: the row bakes the hover wash into its base
`HasColor`, caches the star's entity id per session in a **static map** (it needs
the child id before the child is emitted), and ORs `is_hot(star)||was_hot(star)`
into the row signal.

**Why this is now cheap.** Both halves land in code written today. D13's
`detail::focus_within` already walks UP the parent chain from `focus_id` — the
identical shape over `hot_id` gives `mouse_in_subtree(id)`. And D3's
`ResolveHitTarget::is_candidate` is the single place a hit candidate is decided,
so a `HasInputPassthrough` marker is one condition there. This was listed as
"not doing" in the D3 plan on the grounds nothing needed it; hanabi does.

**DONE.** Both halves landed where predicted.

- `UIContext::mouse_in_subtree(id)` — is the mouse over this element or
  anything inside it. Plus `mouse_was_in_subtree(id)` for the previous frame,
  which is the one to use while BUILDING a screen: `hot_id` for the current
  frame is not resolved until after the screen is built, so the live query
  reads false on the frame the pointer arrives. That is exactly why the app
  workaround ORed `is_hot || was_hot`.
- `with_input_passthrough()` / `HasInputPassthrough` — one condition in
  `ResolveHitTarget::is_candidate`, so the element never becomes hot or active
  and whatever is behind it is hit instead. Distinct from `with_skip_tabbing`,
  which is focus order only; there is a test pinning that difference.

The upward walk now lives on `UIContext::contains_in_subtree` and D13's
`detail::focus_within` delegates to it, so there is one copy rather than two.
`focus_in_subtree(id)` is the same query over `focus_id`.

**hanabi can now delete** the static per-session entity-id map, the baked-in
hover wash on the row's base `HasColor`, and the `is_hot(star)||was_hot(star)`
OR — replaced by one `mouse_was_in_subtree(row_id)` call. That deletion has
not been attempted yet, and per the D3 lesson it is the only real proof the
gap is closed.

### D26. text_input is not a real text field — hanabi's top three are **DONE**
hanabi wrote a full requirements spec (10 sections) and a priority order for its
chat composer. D11/D12 (font size, custom bg, placeholder) were already done.
Their "land these three first" list is now closed:

1. **Control-char filter** (#31) — **DONE, at the root.** macOS sends Backspace
   as a CHAR event carrying DEL (0x7F); the sokol backend queued any
   `char_code > 0` and `insert_char` only rejected `< 32`, so backspace typed a
   blank. Fixed in both places: the backend no longer queues control codes, and
   `is_control_codepoint` (C0 + DEL + C1) is the shared guard every caller
   routes through — the char queue *and* paste, which had its own weaker
   `cp >= 32` check.
   **Proved by deletion:** hanabi's `ComposerCharFilterSystem` (a whole ECS
   system that drained and re-pushed the char queue every frame) and
   `api/textinput_filter.h` are gone, its test now drives `insert_char`
   unfiltered, and 11/11 still pass. Reverting the upstream guard fails that
   test — checked with a forced rebuild, since the first attempt passed only
   because the binary was stale.
2. **Scissor-clip single-line** (#34a) — **was already done**, via
   `Overflow::Hidden` on the field plus the self-clip in
   `compute_intersected_clip_rect`. Nothing tested clipping *anywhere* in the
   suite, so it is now pinned at that function — the one definition both the
   render scissor and hit-testing read.
3. **Multiline wrap + Shift+Enter** (#33/#34b) — **DONE.** See below.
4. Caret origin (#32) — **closed, not taken.** ours tested better on all four
   scenarios; hanabi's branch changes were dropped.
5. macOS Cmd bindings — word/line nav, select-all, clipboard, undo. Present on
   `text_input`, **absent on `text_area`**: the multiline widget still has no
   selection, clipboard or undo at all.
6. Double/triple-click and drag selection. Same split — `text_input` has
   double/triple-click; `text_area` has neither, nor drag.

#### D26.3 — what multiline actually needed
`text_area` never wrapped. It hand-split on `'\n'` and **never read
`word_wrap`** at all, so `with_word_wrap(true)` was dead config — the same
complaint as D8, one widget over.

`text_layout.h` already held a `TextLayoutCache` doing visual-line wrapping,
already sat on `HasTextAreaState`, and **nothing had ever called it**. It was a
second, rival wrap implementation: it broke at different positions than the
renderer (it kept the trailing space in a line), and its `line_at_offset` used
a half-open containment test, so an offset sitting exactly at a line end
matched no line and fell through to the *last* one — which is where the caret
sits every time you press End or type to the wrap point. Rebuilt on
`wrap_text_to_width`, the same primitive both renderers use, so an edited line
now breaks exactly where a drawn one does. The old engine fails 9 of the new
checks.

What the cache adds over the renderer's wrap is the mapping the renderer never
needs: **which source byte each visual line starts at**. A soft break consumes
the space it broke at, so line k does not begin at a position arithmetic can
predict; it is recovered by finding each line's next verbatim occurrence, which
is exact given that the tokenizer preserves everything except break whitespace.
Everything downstream — caret, Up/Down, scrolling — is in *visual* rows now,
because with wrapping on, moving by source line jumps a whole paragraph.

Also added: `with_auto_grow()` (height follows content, capped by
`with_max_lines`) and `with_submit_on_enter()` (Enter sends, Shift+Enter
breaks). Submit-on-enter is **off by default** so a text area keeps Enter
meaning newline.

Found by screenshot, not by test: a grown field was *also* scrolled, showing
only its last row. Auto-grow sets the height for the current frame but the
viewport was still read from the previous frame's computed height, leaving it a
row behind and scrolling to reach a caret that was already on screen. Now
covered.

See wm's `composer_lab` screen. **Still missing on `text_area`:** selection,
clipboard, undo, and click-to-position — it has no click hit-test into the text
at all, only click-to-focus.

### D27. Scroll: anchoring, virtualization, scrollbars
- **#30 scroll anchor / preserve-position-on-prepend.** Content inserted above
  the viewport yanks the view to the top. hanabi holds position by measuring the
  prepended items and bumping `scroll_offset.y` once.
- **#23 no off-screen culling / list virtualization.**
- **#31 virtualization must be built from a STALE scroll offset** — no
  next-offset or velocity hint.
- **#26 `HasScrollView` renders no scrollbar or scroll indicator.**

### D28. Retained layout / dirty-skip
**hanabi #27.** The imm tree is cleared and rebuilt every frame with no
retained-layout or dirty-skip primitive; this is hanabi's idle-frame cost floor.
Big, architectural, and worth its own investigation before any code.

### D29. OS integration: window focus and global hotkeys
**hanabi #28.** No frontmost-app / window-focus query, so no focus-gated global
hotkey. Sits with D21 (OS appearance) as the "platform shim" ask.

### D30. Container widgets floatinghotel still hand-rolls — **DONE**
Was the only gap in any doc still marked blocking.
- **Draggable divider** — `imm::divider(ctx, mk(...), Axis)`. Truthy on the
  frames it moved; `.as<float>()` is that frame's travel along its own axis, in
  the same space as `rect()`. Sizes itself thin and sets the resize cursor.
- **Split pane** — `imm::hsplit_pane` / `imm::vsplit_pane`, taking a `float&`
  ratio the drag updates in place, and returning `{first, divider, second}`.
  The divider comes back so it can be styled rather than configured. See wm's
  `split_pane_lab` screen and `93_split_pane_dividers.e2e`.
- ~~Tree node (P1)~~ — **afterhours already ships `ui/tree_view.h`**, with
  `TreeNode<T>` and `HasTreeViewState`. floatinghotel has no tree view at all
  any more. Not a gap; an adoption question at most.

The divider exposed two defects underneath it, both fixed here:

**`HasDragListener::down` was never written.** Nothing in the library set it,
so every caller polling it outside the callback — which is the only way an imm
widget can react to a drag while rebuilding — saw false forever.
floatinghotel's two dividers (`main_content_system.h`, `sidebar_system.h`) are
both `if (drag.down)` and have therefore never worked. `HandleDrags` now sets
it, mirroring `HasClickListener`.

**No delta, and no stated coordinate space.** Callers reached for
`graphics::get_mouse_position()` and converted by hand (floatinghotel does
`mouseX * 1280.0f / sw` in both copies; wm's resize box is a third). They did
not have to: `ctx.mouse.pos` has always been in `rect()` space, because
`input::get_mouse_position` already undoes letterboxing and resolution scale.
What was genuinely missing is the frame delta, so `ctx.mouse.delta` is now
alongside it — derived from `pos`, NOT from the backend's mouse delta, which is
raw window pixels and would reintroduce the conversion it exists to avoid.

**Found while building the lab screen, and the reason it rendered nothing:**
`solve_violations` skips absolutely-positioned children, correctly, because
their size must not feed their parent's — but it skipped their whole *subtree*
with them, and expand is only ever resolved in there. So any `expand()` under
an absolutely-positioned element collapsed to 0 while its `percent()` sibling
was fine. Fixed in `autolayout.h` and covered by two tests; wm's 81 screenshots
are byte-identical across the change, so nothing was relying on the collapse.

Not done: `restyle` carries a background but not `roundness`, which only lands
alongside an explicit corner set. Small, and unrelated to panes.

### D31. Styled text: no wrap, no weight — **DONE**
**hanabi #22 + follow-up.** floatinghotel filed the same two as "No Font Weight
Support" and "No Rich Text". Related: D8, D9.

- **Wrap** — done earlier, in the multi-line text work. `with_styled_label`
  word-wraps and honours hard `\n`; `draw_runs_in_rect` goes through the same
  single wrap primitive as plain text.
- **Weight** — `TextSpan` now carries a `colors::FontWeight`, resolved per run
  through `FontManager::resolve_weighted` (base + `@bold`). A span's weight
  wins over the component's; a component with no per-span weights resolves
  exactly as before. An unloaded variant falls back to the base face, so the
  feature is adoptable one font at a time.

Weight had to reach the *wrapper*, not just the draw: bold glyphs are wider, so
measuring a bold run with the regular face under-measures the line and it
overruns. `wrap_runs_to_width`'s measure is now `measure(text, weight)`, called
on the largest same-weight stretch of a candidate — a uniform line is still one
call, so the no-drift property the whole-candidate measurement exists for is
preserved, and only a genuine weight boundary costs a join.

**Nothing exercised any of this before.** `weight_suffix`, `resolve_weighted`,
`UIComponent::font_weight` and `with_font_weight` all existed, but no app ever
registered an `@bold` font, so `resolve_weighted` always fell through to the
base. wm now bundles `DGOne` / `DGOne@bold` (Oldschool PC Font Pack, CC BY-SA
4.0) purely because it was the only genuine same-family weight pair available —
every other bundled bold is a different typeface, and the one true bold,
Gaegu-Bold, *is* the default face. See the `styled_text_lab` screen.

Found while doing this, **not fixed**: `draw_runs_in_rect` (imm) started
Left-aligned styled text flush at `rect.x` while the plain path insets by the
5px margin, so giving a label a colour shifted it 5px left. Fixed for imm and
covered by a test. The batched renderer insets *neither* plain nor styled, so
it is internally consistent but sits 5px left of imm for both. That imm/batched
divergence is pre-existing and untouched — closing it would move every plain
label in every app by 5px.

### D33. `focus_ring_thickness = 0` did not disable the focus ring — **DONE**
**puzzle.** Four defects that together forced the app to hand-paint its own
ring across three screens. Repros: `downstream_gaps_test.cpp` d33/d34.

- **`focus_ring_thickness = 0` did not disable it.** Batched routed 0 to
  `render_rounded_outline_batch`'s `else` (1px line); both renderers drew the
  contrast outline unconditionally. puzzle set 0 to opt out and shipped a
  doubled ring for a day without noticing — which is the real severity here.
(D34 covers the other three.)

### D34. Focus ring: invisible under a fill, unpositionable, unseen on a filled widget — **DONE**
**puzzle.** Repro: `downstream_gaps_test.cpp` d34.

- **The immediate renderer drew the ring UNDER the widget fill.** `focus_rect`
  is inset inside `draw_rect`, so any opaque `HasColor` covered it — every
  default `button()`. Batched was correct (layer+199/+200); the two copies had
  drifted.
- **No outset, no sub-pixel offset.** `focus_rect(int)` truncated the float
  theme value. A negative offset did outset, but `theme.h`'s comment said it
  couldn't.
- **Contrast outline was outside-only**, so an amber ring on an amber-filled
  widget vanished.

Fixed by extracting `detail::focus_ring_for()` — the ~95 lines were duplicated
between `RenderImm` and `RenderBatched` and had drifted four ways. The
`FocusClusterRoot` fallback (2 levels vs. `ComputeVisualFocusId`'s 64) is gone
with it. Prerequisite: the `none` backend's
`draw_rectangle_rounded_lines[_ex]` were `@notimplemented` stubs, so no outline
— border or focus ring — was observable in tests at all.

### D35. Hover-follow silently overwrote keyboard focus — **DONE**
**puzzle.** `ComputeVisualFocusId` runs after every user system and, under
`FollowsMostRecentInput`, re-grabs focus for whatever is hot. So a game moving
focus in its build has it undone the same frame whenever the cursor is resting
on a widget — which on a menu is always. puzzle's arrow keys did nothing on its
level list. No suppression hook existed; `set_focus` was a bare setter.

`FocusSource { Grab, Pointer, Explicit }`, recorded by `set_focus` (default
`Explicit`), reset per frame; hover defers to an `Explicit` claim. The
Grab/Explicit split is load-bearing — `try_to_grab` sets focus every frame, so
treating that as intent would kill hover-follow outright. Also un-breaks
`HandleFocusUICommand`, which was being undone identically.

Related: `mouse.moved_this_frame` was an exact float compare — sub-pixel jitter
read as intent, and a NaN position (no cursor: every headless run) reported
movement every frame forever. Now a dead-zone distance test with both
non-finite cases decided explicitly; a plain distance test is also wrong, since
one NaN frame would then poison `prev` and report *no* movement forever.

### D36. Arrows are welded to Tab; `AcceptsValueInput` was dead — **DONE**
**puzzle.** `process_tabbing` assigns `WidgetDown` into the same `forward` as
`WidgetNext`, so "arrows within a group, Tab between groups" was unreachable.
The documented escape hatch had three references in the library, all
declarations — nothing attached it, no config setter existed.

Shipped casualty: **a Column tray cannot be arrow-navigated at all**, because
`process_tabbing` consumes `WidgetDown` nine systems before
`HandleTrayNavigation` looks. Dropdown option lists are Column trays, and
nothing covered it.

Renamed the component to `ConsumesDirectionalInput` (`AcceptsValueInput` kept
as an alias so the five downstream consumers keep compiling) and exposed
`with_consumes_directional_input()`; `tray()` sets it. The rename is the point:
a tray stepping its children is not adjusting a value, and the "value" framing
is why nobody ever wired the hatch up. Added `theme.arrows_tab` (default true)
so an app with its own directional nav can opt out instead of racing for a
single-slot `last_action`. Also: `HandleTrayNavigation` now follows `hot_id`, so
clicking a tray item no longer leaves `selection_index` on the previous one.

Note there is **no `ValueUp`/`ValueDown` action pair** — navigation and value
adjustment share `WidgetUp`/`WidgetDown` and this component is the only thing
separating them. Adding real value actions later collides with it silently,
since `process_tabbing` resolves actions by name via `if constexpr`.

Scope caveat: those `if constexpr` guards mean this only ever affected apps
whose `InputAction` declares `WidgetUp`/`WidgetDown`.

**Verified against wm_afterhours** (pinned `14222a3`, patch applied in a scratch
copy): `tray_vertical_navigation` goes **FAIL → PASS**. Before, Down tabbed out
of the vertical tray — "Expected visual focus on 'V-Beta', but visual focus is
on 'H-Alpha'". `validate-screenshots` is unaffected: same 8 pre-existing
failures before and after, with three moving by ≤0.0014 percentage points from
the new inner contrast edge.

**Why it was never caught:** `wm_afterhours/src/testing/tests/all_tests.h`
includes only `FontConfigTest.h`. `TrayTest.h`, `TabbingTest.h`,
`RadioGroupTest.h` and four others are in the directory but compiled by nothing.
Adding two `#include`s takes that suite from 7 tests to 12 and turns this bug
red immediately.

### D37. e2e `push_key` used a one-reader-per-frame queue — **DONE**
**puzzle.** `test_input`'s queue has a global `key_consumed` latch, while
`InputSystem` polls each action 3x per gamepad id — so a queued key reached
whichever caller asked first and nobody else. Worked in a minimal example,
silently dead once a second binding/hotkey/controller existed. puzzle saw Tab
assertions pass headless while the feature did nothing windowed.

`push_key` now goes through `input_injector` (explicitly multi-reader, and its
docstring already named this hazard); the queue stays for `push_char`. The
injector press lands a frame later, so
`input_injector_mouse_delta_test`'s interleaving case was rewritten — the
property it protected is now true by construction.

Still open: `register_ui_commands` has no caller **inside this repo**, but five
downstream — `floatinghotel/src/main.cpp:352`,
`kart-afterhours/src/e2e_integration.h:76`, `wm_afterhours/src/game.cpp:852` and
`.../testing/e2e_integration.h:40`, `wordproc/src/testing/e2e_integration.h:56`.
So it is load-bearing: call it from the e2e setup entry point or document it as
required — do NOT remove it. An app that forgets it gets "Unknown command" with
no hint the handlers exist.

### D38. Focus groups, tray hover, scroll-into-view — **OPEN, needs a decision**
**puzzle.** Three things left deliberately unfixed:

- **Hover cannot reach tray children.** They are force-marked
  `SkipWhenTabbing` and `can_be_focused` — shared by tabbing and hover — rejects
  those. The tempting fix (split into tab/pointer predicates, hover ignores the
  flag) is **wrong**: `with_skip_tabbing` is public API meaning "not
  reachable", and puzzle uses it for locked rows and a disabled menu item that
  must not become hover-focusable. The narrower fix is for tabbing to skip tray
  children structurally (parent has `HasTray`) rather than branding them with a
  public opt-out.
- **No scroll-into-view.** `HandleScrollInput` is gated on the cursor and never
  reads `focus_id`; Tab can focus a row scrolled out of sight.
- **No focus groups.** `HasTray` (one tab stop, children unfocusable) and
  `FocusClusterRoot` (cosmetic) are the only grouping primitives. "Arrows scope
  to a group, Tab moves between groups, children stay mouse-focusable" lives in
  the app (`menu_section_nav`). A `FocusGroup` marker + group-aware
  `process_tabbing` would delete it. Feature, not bug.

### D32. wm's remaining open items (all low priority)
Slider knob 0.75 compression (cosmetic); crowded tab bars still need a smaller
font at the call site; word-wrap has no hard character break, so a single word
wider than its box is not split.
