# Library follow-ups

The source-comment inventory below is historical and needs a current reproduction
before work starts. Line numbers may have moved. Do not regenerate resolved gaps from
old consumer pins. Original detail remains at `c93e10e:todo.md`.

## Build tests correctly

```sh
nice -n 10 make -C tests -j2 test
```

Input-injection tests need `AFTER_HOURS_ENABLE_E2E_TESTING`; otherwise widget reads
bypass injected state. Use the existing Makefile targets or
`tests/run_mouse_delta_backend_checks.sh`. Six supposed text-area defects were
misbuilt tests, not widget failures. The old request to add a test Makefile is resolved.

## Pending consumer work

- D2/D16: obtain current minimal reproductions for ordinary row expand and nested
  percent sizing before changing the solver. Existing covered cases pass.
- D7: recheck nested-scroll visibility on the affected consumer/backend. Separate
  decoration clipping from hidden children and unresolved content heights.
- D9: document font coverage and register needed ranges/fallbacks. Non-ASCII text
  is not automatically present in an atlas; do not infer a renderer failure from a missing glyph.
- D10: verify layout, measurement and pointer injection share logical coordinates
  through HiDPI/headless conversions. Public conversions need explicit units.
- D14b: disabled text darkens separately from disabled backgrounds; disabled_opacity
  affects the latter. Unifying appearance is a separate reviewed visual change.
- D21/D29: portable OS appearance, window-focus and global-hotkey integration need
  explicit availability and platform lifecycle. Keep app policy outside adapters.
- D22: exit retention, stagger/delay, value-change triggers, shimmer and drag-to-spring
  transitions remain candidates. Existing animation APIs cover ordinary hover/press/appear/loop.
- D23: consider Tween/AnimatedValue convenience, synchronized scrolling and font tiers
  that respect Adaptive zoom. Virtual lists already exist; do not rebuild them.
- D25: recheck subtree hover and hit-test exclusion against current APIs and consumer
  behavior. Hover policy must not silently change the meaning of explicit opt-outs.
- D27: scroll anchoring/preserve-on-prepend and next-offset/velocity hints need review.
  Scrollbars, virtual rows and keyboard reveal already exist.
- D28: retained-layout/dirty-skip needs a measured workload and complete invalidation.
- D38: decide whether groups should use arrows internally and Tab between groups while
  keeping children pointer-focusable. Tray structural skipping must not override public
  skip-tabbing intent. Keyboard scroll-into-view was subsequently implemented.
- D32: slider knob compression, crowded tab labels and hard breaks inside long words
  remain low-priority presentation/layout questions.
- Split region `{size, config}` pairs remain deferred because `restyle` already covers
  styling. Add them only if real callers need the locality.
- E2E setup must register UI commands. Do not delete registration just because all
  callers are downstream; improve diagnostics or the common setup contract.
- Recheck historical text-area Cmd bindings, selection, clipboard, undo and pointer
  placement against current implementation. The old blanket missing-feature claim is stale.

## Resolved historical reports

D1 alpha blending; D3 overlapping click priority; D4 NoWrap default; D5 missing-child
crashes; D6 ellipsis nontermination; D6b real-renderer harness; D8 label wrap; D11/12
text-input styles/placeholder; D13 menus/popovers; D15 pixel radius; D17 text sizing;
D18 shared wheel reads; D19 HiDPI headless capture; D20 mipmaps; D24 unreproduced
widget reports; D26 control filtering/wrapping; D30 split/container widgets; D31 styled
wrap/weight; D33–37 focus and input fixes. Revalidate consumers before deleting adapters.

The hello-world pass added string configs, min/max sizes, splits, UI setup/default
input actions, theme files and `ui::run`. Window presentation and catalog dependency
tracking were fixed after screenshots of offscreen textures missed both defects.
Layout caching, mass setter renaming, a mandatory Widget trait and a prelude namespace
were not part of that pass. Research is consolidated in [the roadmap](docs/roadmap.md).

## Source-comment candidates

These retained notes include informational workarounds and possibly completed work;
they are not a claim that every listed API is still missing.

### Core

### Library (`src/library.h`)

- Line 101: Feature; Random Generator; Random generator is stubbed out; always uses index 0 instead of a proper random selection from matched results

### Bitset Utilities (`src/bitset_utils.h`)

- Line 90: Code Quality; Combine Functions; Two related utility functions should be merged

### Developer Tools (`src/developer.h`)

- Line 68: Architecture; Code Organization; Developer tools should be moved to a dedicated file

### Plugins

### Color (`src/plugins/color.h`)

- Line 177: Architecture; Vector Type Abstraction; Consider using `#ifdef VECTOR_TYPE` to avoid hardcoding vector type

### Texture Manager (`src/plugins/texture_manager.h`)

- Line 52: Code Quality; Duplicated Code; Code was copied from transform component; should be refactored to share
- Line 88: Feature; Text Alignment; Need support for `InnerLeft` and `InnerRight` alignment options

### AutoLayout (`src/plugins/autolayout.h`)

- Line 575: Configuration; Fallback Behavior; Questioning if fallback behavior should be a configurable setting
- Line 716: Layout Logic; Flex Children; Unsure about applying sizing logic to non-1.0 flex children
- Line 812: Layout Constraint; Cannot enforce size assertions when text wrapping is enabled

### Modal (`src/plugins/modal.h`)

- Line 47: Architecture; Config Integration; Should use `ComponentConfig` for modal configuration
- Line 317: Feature; Configuration Support; Add support for modal configuration options

### Input System (`src/plugins/input_system.h`)

- Line 106-109: Informational; macOS Workaround; `raylib::IsMouseButtonPressed` is broken on macOS because `glfwSwapBuffers` pumps the Cocoa event queue, setting `currentButtonState` before `PollInputEvents` copies current→previous. Manual edge detection via `IsMouseButtonDown` is used instead. (Active workaround, not a TODO to fix.)
- Line 236: Feature; Controller Support; Currently only using Xbox controller button names; need PlayStation and other controller icons
- Line 272: Feature; macOS Icon; Need macOS-specific icon for super/command key
- Line 482: Informational (Raylib Upstream); `KEY_MENU` maps to the same value as `KEY_R`. Raylib 5.5 adds `KEY_MENU` for Android; potential upstream conflict.
- Line 643: Feature; Mouse Position; `get_mouse_position()` is not implemented ("good luck")
- Line 772: Feature; Configurable Deadzone; Gamepad deadzone (0.25) is hardcoded; should be user-configurable
- Line 894: Architecture; Singleton Query; `get_input_collector()` should use a singleton query pattern
- Line 904: Architecture; Namespace; Input system struct should be moved out of the `input` namespace

### E2E Testing

#### Input Injector (`src/plugins/e2e_testing/input_injector.h`)

- Line 10: Architecture; Input Parity; E2E input injector should match 1-to-1 with the UI input system to share code

#### Pending Command (`src/plugins/e2e_testing/pending_command.h`)

- Line 111: Design Decision; Error Handling; Should pending command failures throw exceptions or auto-fail?

#### Runner (`src/plugins/e2e_testing/runner.h`)

- Line 129: Feature; Plugin Registration; Add a way for plugins to register their own e2e testing commands

#### UI Commands (`src/plugins/e2e_testing/ui_commands.h`)

- Line 66: Feature; Conversion Helper; Add a `gen_lambda()` helper for entity conversion
- Line 367: Feature; Slider Calculation; Calculate percentage from slider min/max when `HasSliderState` supports it
- Line 410: Feature; Wait + Click; Add wait and option-click logic for e2e tests

### UI System

#### Context (`src/plugins/ui/context.h`)

- Lines 3-9: Architecture; C++20 Concepts; Consider using C++20 concepts for type constraints (see `e2e_testing/concepts.h` for examples)
- Line 69: Architecture; InputBitset Coupling; `InputBitset` definition should move to input system; currently creates a dependency on `magic_enum` in the UI layer

#### Theme (`src/plugins/ui/theme.h`)

- Line 61: Architecture; Font Identification; Investigate using a `FontID` enum instead of strings for type safety

#### Component Config (`src/plugins/ui/component_config.h`)

- Lines 29-36: Architecture; Config Splitting; Consider splitting monolithic `ComponentConfig` into concept-constrained configs per component type (e.g., `TextInputConfig` only exposes text-input-relevant methods)
- Line 87: Design Decision; Inheritance; Should all component config properties be inheritable?
- Line 548: Code Quality; Rename; Rename method to `is_absolute()` for clarity

#### Components (`src/plugins/ui/components.h`)

- Line 114: Architecture; State Unification; Consider unifying `HasStepperState` and `HasDropdownState` since a stepper is essentially a dropdown variant
- Line 520: Code Quality; Magic Numbers; Build more confidence around how to set numeric values to avoid issues
- Line 534: Feature; Drag Groups; Consider adding named drag groups and accept-list filtering

#### Entity Management (`src/plugins/ui/entity_management.h`)

- Line 60: Feature; Element Tracking; Add a count of how many UI elements are created

#### Rendering (`src/plugins/ui/rendering.h`)

- Line 337: Performance; Caching; Rendering system needs caching for better performance
- Line 1444: Note; Self-referential note indicating TODO might not be needed

#### Core Components (`src/plugins/ui/ui_core_components.h`)

- Line 263: Configuration; Default Values; Uncertain about default spacing values (5,5 vs 10,10)

#### Systems (`src/plugins/ui/systems.h`)

- Line 69-70: Architecture; Wrong Module; Function should live inside `input_system` but doing so would require `magic_enum` as a dependency there
- Line 279: Architecture; System Filter; Should move logic to a system-level filter
- Line 367: Architecture; Tag Support; Template approach works but wishes it worked better with Tags without requiring `UIComponent` in `for_each_with`
- Line 771: Feature; Repeat Rate; Consider using a different key repeat rate for WidgetLeft
- Line 811: Feature; Repeat Rate; Consider using a different key repeat rate (duplicate context)
- Line 926: Architecture; Side Effects; Figure out if current approach will actually cause trouble
- Line 955: Architecture; Pound Define; Replace magic number validation (options > 100) with a `#define`
- Line 1177: Performance; Inlining; Consider inlining drag-tag query helpers
- Line 1200: Feature; Gen For Each; Need a `gen_for_each()` or equivalent for tag operations
- Line 1244: Feature; Deep Clone; Only flat properties (HasLabel, HasColor) are copied during drag. Dragged items with children won't render correctly; consider deep-cloning subtree
- Line 1549: Architecture; Tag Conversion; Consider converting `was_rendered_to_screen` to a tag
- Line 1552: Code Quality; Combine Checks; Combine `should_hide` and `ShouldHide` tag checks
- Line 1581: Feature; Natural Scroll; Add support for customizing scroll direction ("natural" scroll)

#### Immediate Mode Components (`src/plugins/ui/imm_components.h`)

- Line 65: Architecture; Namespace; Consider moving existing primitives (div, button, sprite, image) into a `primitives` namespace
- Line 67: Architecture; Namespace; Consider whether stateful convenience wrappers should live in a separate namespace
- Line 497: Code Quality; Button Wrapping Hack; Current approach to get buttons to wrap is a hack; needs a cleaner solution
- Line 1123: Bug; Slider Overflow; `slider_background` can overflow by ~1-2px when parent is constrained
- Line 1222: Feature; Slider Handle Height; Support custom handle height via a dedicated config field
- Line 1448: Architecture; Hot Sibling; Summary of previous label-checkbox interaction behavior that changed
- Line 1536: Feature; Tag Setter; Add a way to set tags directly from a bool
- Line 1549: Architecture; Navigation Bar; Consider making `navigation_bar` a thin wrapper around existing components
- Line 1578: Feature; Default Values; Add defaults for configuration
- Line 2304: Feature; Neighbor Styling; Make neighbor styling configurable (muted color, smaller font, etc.)

#### Text Input (`src/plugins/ui/text_input/component.h`)

- Line 227: Feature; Horizontal Scrolling; Implement horizontal scrolling when text exceeds field width

#### Setting Row (`src/plugins/ui/setting_row.h`)

- Line 295: Architecture; Reusable Component; Add a component for this instead of building one inline

#### Styling Defaults (`src/plugins/ui/styling_defaults.h`)

- Line 101: Architecture; Singleton Helper; Needs a singleton helper pattern

### Backends

### Sokol Drawing Helpers (`src/backends/sokol/drawing_helpers.h`)

- Line 209: Feature; Rotation Support; Drawing helpers lack rotation support
- Line 338: Feature; Thick Lines; Implement thick lines via quads

---
