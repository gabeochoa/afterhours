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

Both gap-doc apps are on the **sokol** backend, so their reports skew there;
wm_afterhours exercises raylib. Neither app has ever patched vendor — every
item below has a shipped app-side workaround, which is why none of it has
surfaced as a bug report.

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

### D1. Sokol backend has alpha blending disabled — 3 symptoms, 1 fix
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

### D4. `FlexWrap` defaults to `Wrap`
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

### D11. text_input ignores `with_font_size` and `with_custom_background`
**hanabi #17.** The widget *derives* its font size from computed field height
(`text_input/component.h:187`, `derived_fs = field_h * 0.5f`) and forces the
inner field fill to `Theme::Usage::Secondary` (`:163`). Both config calls are
silently ignored: a 72px field rendered hanabi's draft at ~36px and overflowed,
and the field stayed dark even in the app's Light theme. Workaround: pin the
field to ~34px so the derived font lands readable, and accept the wrong colour.

Ask: honour an explicit `with_font_size` when set, falling back to the derived
size only when unset; let `with_custom_background` override the forced fill.

### D12. text_input has no `with_placeholder`
**hanabi #17 follow-on (2026-08-01).** Grep-confirmed absent. An empty field is
a bare box — hanabi's sidebar search read as an unlabelled box + magnifier and
was written up as a hostile-review defect. Compounded by D11: because the field
force-fills opaque `Secondary`, a placeholder painted *behind* the input is
covered by the field's own fill, so the obvious workaround doesn't work either.
hanabi ships an absolutely-positioned overlay child at a higher render layer,
with its origin hand-derived from panel geometry (panel xy → header height →
search-wrap/field paddings + magnifier slot). Fix D11's forced fill and this
becomes a ~10-line addition.

### D13. Missing container/menu primitives
**floatinghotel missing-primitives #1, #2, #4, #5, #6.** Each is built app-local
in `src/ui/`, all five marked BLOCKER or HIGH:

| Want | floatinghotel's local build | Status here |
|---|---|---|
| `draggable_divider()` | `split_panel.h` via `div()` + `HasDragListener` | missing |
| `split_pane()` | `split_panel.h` | **`vsplit`/`hsplit` (todo item 6) covers the static case**; the draggable divider is what's left |
| `dropdown_menu()` | `menu_setup.h`, absolute positioning + hover-to-switch + click-outside | missing |
| `context_menu()` | `context_menu.h`, cursor-positioned, window-edge flipping | missing |
| `popover()` | reuses the dropdown approach with manual anchor maths | missing |

The last three share one engine: an anchored, auto-flipping, click-outside-to-
close overlay with a common item format (label, shortcut, separator, disabled,
callback). Build that once and all three fall out. Note D3 blocks nothing here
but will bite any of them that nest a control inside a clickable row.

### D14. Custom colours bypass disabled dimming — **DONE**
**floatinghotel.** `with_disabled(true)` blocked interaction but left the
widget looking enabled. An accessibility bug, not a cosmetic one: a control
that looks enabled invites clicks that silently do nothing. Their workaround
was hand-picking colours in every preset factory.

**The report named the wrong function** — same shape as D5. It points at
`ComponentConfig::resolve_background_color()` (`component_config.h:818`), which
does have the bug but is only reached from `imm_components.h:91`/`:619`, for
non-Filled button variants. The live path for every widget is
`component_init.h:362-374`, which never calls it and re-implements the same
branch with the same mistake. Fixing only the named function would have left
the bug in place.

**Fix:** extracted the dim transform out of `Theme::from_usage` into
`Theme::disabled_variant(Color)` (`theme.h`) — mix toward background, scale
alpha by `disabled_opacity`, desaturate 50% — and called it from *both*
custom-colour paths. `from_usage` now delegates to it, so the pre-existing
theme path is unchanged by construction.

Free consequence: `component_init.h:464-466` sets `HasLabel::background_hint`
from `HasColor` *after* the colour is assigned, so auto-contrast text
re-derives against the dimmed background with no extra work.

**Tests** (`downstream_gaps_test`): `d14_disabled_custom_background_is_dimmed`
is the regression guard and fails before the fix;
`d14_enabled_custom_background_is_untouched` is the control that catches
dimming everything; `d14_disabled_theme_background_still_dims` pins the path
the refactor touched.

**Also de-duplicated** the disabled-text logic. `RenderImm` and `RenderBatched`
carried identical copies, so the darken-when-disabled line existed four times.
Now one `detail::resolve_label_color()`.

### D15x. hanabi's two vendor patches — **DONE**, one taken, one rewritten
hanabi ships `vendor_patches/`: fixes prototyped in their app and captured
"ready for the maintainer". Reviewed both rather than applying blind.

**#25 (sokol rounded corners) — taken as-is.** `emit_corner_arc`'s sharp branch
called `sgl_begin_triangles()` and emitted *two* vertices: a degenerate
primitive that rendered as a diagonal slice on any mixed round/sharp config.
Verified the claim by tracing `rTL == 0`: the top edge starts at `(x+rTL, y)`
and the left edge ends at `(x, y+rTL)`, both `(x, y)`, both fanning from the
centre — the square corner is fully tiled without the arc. Raylib handles mixed
corners via its own path, so this was sokol-only. No test: no sokol target
exists here (D1), and saying so beats implying coverage.

**#22 (styled spans word-wrap) — intent accepted, implementation rewritten.**
The need is general: `with_styled_label`/`TextSpan` already shipped, and
floatinghotel asked for the same API by name. So this finishes a half-built API
rather than adding one. But the patch had three problems:
- It only touched `RenderBatched`. Spans were handled at exactly one site, so
  `with_styled_label` was a **silent no-op** under `RenderImm` — selectable at
  runtime via `use_batched` with nothing in app code mentioning it.
- Its wrap math summed per-word widths while the real wrapper measures the
  whole candidate line. `measure_text` applies spacing *between characters*, so
  the sum drops two gaps per word boundary and drifts — breaking the very
  height-model guarantee the patch claimed to provide.
- It inherited #24.

Instead: `detail::wrap_runs_to_width` is now the single wrapping primitive, and
`wrap_text_to_width` is one colourless run through it. Plain and styled break
identically **by construction**. Both renderers use it (`draw_runs_in_rect` for
`RenderImm`), and `d22_both_renderers_agree_on_styled_output` pins that.

Also found: a wrapping styled label did not render styled-but-unwrapped — the
wrap branch set `wrapped` first, making the span branch unreachable, so it
rendered as **plain text with colours discarded**.

**#24 (`\n` ignored in wrapping) — DONE**, folded in since it is the same
function. `\n` is a hard break; `\n\n` yields a blank line so paragraph spacing
survives.

### D22b. `with_font_size()` alone was silently ignored — **DONE**
Not from any app report. Found because a plain-text wrap baseline that should
obviously have passed did not.

`component_init.h` gated `enable_font()` — the only thing that copies
`font_size` and `font_size_explicitly_set` onto the `UIComponent` — on
`config.font_name != UNSET_FONT`. Set a size without also naming a font and
`explicit_fs` resolved to 0, switching off every path that needs a known size.
Wrapping was the visible casualty. Confirmed both ways: same widget, no font
name → one 752px line in a 230px box; add `with_font(...)` → wraps to four
lines.

Strong candidate for floatinghotel footgun F1 ("labels don't word-wrap"): set a
size, ask for wrapping, get one clipped line and no diagnostic. Worth telling
them.

### D1b. There is now a working native sokol/Metal target — **DONE**
D1, D19 and D20 were all blocked on "nothing here builds sokol". That was
nearly untrue: `examples/web` already had a native macOS/Metal recipe
(`make demo`) alongside its emscripten one. It just did not work.

Two blockers, both fixed:
- **Link failure.** `sokol_impl.cc` never included `capture_impl.h`, so
  `metal_create_system_device` and `metal_capture_render_texture` were
  undefined. It also has to `#define AFTER_HOURS_USE_METAL` — `main.cpp`
  defines that for itself, and `capture_impl.h` is guarded on it, so including
  it alone would still have compiled to nothing. Guarded on `SOKOL_METAL` so
  the WebGL2 build is unaffected.
- **Segfault on startup**, hit the moment it linked. `RenderSprites::once`
  (`texture_manager.h:225`) did
  `get_singleton_cmp<HasSpritesheet>()->texture` with no null check, so any app
  registering that system without a spritesheet crashed;
  `RenderAnimation::once` was identical. Both now resolve the sheet in
  `should_run` and skip when absent. Same class as D5.

`make demo` builds, links and runs. **Not yet a test target** — it is windowed
with no headless mode, so it proves the backend works but cannot make
assertions. A headless sokol test (offscreen render + `capture_render_texture_to_memory`,
which already exists) is the remaining step before D1/#25 can be verified
automatically rather than by eye.

### Porting wm_afterhours to sokol — **not recommended**
It is raylib-only: no metal/sokol in its makefile, no sokol impl TU, and
**445 direct `raylib::` calls across 32 files**. That is a port, not a
configuration change, and `examples/web` already gives a sokol target for a
fraction of the cost.

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

### D19. Metal headless capture can't supersample
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
