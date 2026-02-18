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
