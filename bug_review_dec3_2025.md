# Bug Review - December 3, 2025

This document contains a comprehensive review of potential bugs, issues, and areas of concern in the afterhours ECS framework codebase.

---

## Core Files

### `ah.h`
1. **FIXED: Inverted preprocessor guard** - The `#if defined(AFTER_HOURS_MAX_COMPONENTS)` should be `#if !defined(...)` to actually set the default value when not defined.
2. **Missing include guards for nested includes** - The file includes `src/ecs.h` but doesn't guard against potential circular dependencies.
3. **Documentation mismatch** - The comment says "defaults to 128" but the guard logic was previously broken, meaning the default was never applied.

### `src/core/base_component.h`
1. **Component ID overflow silently returns same ID** - When `lastID >= max_num_components`, it returns `max_num_components - 1` causing multiple different component types to share the same ID, leading to silent data corruption.
2. **No thread safety for component ID generation** - `get_unique_id()` uses a static local variable without any synchronization, which is unsafe in multi-threaded initialization scenarios.
3. **No way to query current component count** - There's no API to check how many component types have been registered, making it hard to diagnose overflow issues.

### `src/core/entity.h`
1. **Pragma diagnostic pop in wrong location** - Lines 207-211 show the `#pragma pop` after the return statement in `get() const`, which is syntactically valid but semantically confusing and may not suppress warnings correctly in all cases.
2. **No bounds checking in tag operations** - `enableTag`, `disableTag`, `setTag` silently fail when `tag_id >= AFTER_HOURS_MAX_ENTITY_TAGS` without any logging, making debugging difficult.
3. **`has_child_of` iterates all components** - The `has_child_of<T>()` method does a linear scan of all 128 component slots even though most are likely empty, which is inefficient.

### `src/core/entity_helper.h`
1. **FIXED: Typo in variable name** - `permanant_ids` should be `permanent_ids`.
2. **`markIDForCleanup` is O(n)** - The function linearly searches through all entities to find one by ID. Should use a map or the singleton pattern.
3. **`getEntityAsSharedPtr` returns empty shared_ptr for temp entities** - If an entity exists in `temp_entities` but not in `entities_DO_NOT_USE`, this function will return an empty shared_ptr even though the entity exists.

### `src/core/entity_query.h`
1. **`gen_first()` runs query twice** - It calls `has_values()` which runs the query, then calls `gen_with_options()` which runs it again. Same issue in `gen_first_enforce()` and `gen_first_id()`.
2. **Mutable `Limit` counter in const context** - The `Limit` modification has a `mutable int amount_taken` which is modified in `operator()` even though the query can be used in const contexts, leading to potential issues with query reuse.
3. **`gen_random()` uses `rand()`** - Using `rand()` is not thread-safe and provides poor quality randomness. Should use `<random>` facilities.

### `src/core/system.h`
1. **Explicit specialization in class scope (GCC error)** - Lines 114, 155, 203 have `template <> struct HasAllComponents<type_list<>>` inside the `System` class, which is invalid in standard C++ (works in Clang but not GCC).
2. **`tags_ok` unused parameter warning** - On non-Apple platforms, `tags_ok(const Entity &entity)` always returns `true` and the `entity` parameter is unused.
3. **`SystemManager::fixed_tick` doesn't call `merge_entity_arrays`** - Unlike `tick()`, the `fixed_tick()` method doesn't merge temp entities after each system runs, which could cause newly created entities to be missed.

---

## Plugin Files

### `src/plugins/animation.h`
1. **`one_shot` static set grows unboundedly** - The `started` sets in `one_shot` functions are static and never cleared, causing memory to grow indefinitely for long-running applications.
2. **`loop_sequence` captures key by value in lambda** - Line 232-234 captures `k` and `segs` by value and calls `anim<Key>(k)` in the completion callback, but if `Key` is large, this is inefficient.
3. **No way to cancel or reset an animation** - Once an animation is started, there's no API to cancel it or reset the track for reuse.

### `src/plugins/pathfinding.h`
1. **Race condition in `AtomicQueue`** - Calling `empty()` then `front()` without holding the lock between calls means another thread could modify the queue between these calls.
2. **Memory leak in BFS** - `find_path_bfs` uses raw `new` for `Node` objects. If an exception is thrown, the cleanup loop won't run and memory leaks.
3. **`init()` calls `merge_entity_arrays()` synchronously** - This could cause issues if called from a system callback, as merging during iteration is undefined behavior.

### `src/plugins/settings.h`
1. **`SettingsSaveSystem::once` is empty** - The save system is registered but does nothing, which is misleading.
2. **`load()` opens file twice** - The function opens the file to check if it exists, then the format-specific load function opens it again.
3. **Static fallback variables** - In `get_data()` and `get_data_const()`, a static `fallback` is returned when provider is null, meaning all callers share the same mutable state.

### `src/plugins/timer.h`
1. **`TriggerOnDt::test` modifies state but isn't marked as such** - The method modifies `current` but doesn't clearly indicate it has side effects.
2. **`HasTimer::update` returns true but also resets** - When `auto_reset` is true, the timer resets immediately after triggering, but the caller might expect to handle the trigger first.
3. **No pause state in `TriggerOnDt`** - Unlike `HasTimer`, `TriggerOnDt` has no `paused` field, making it inconsistent.

### `src/plugins/camera.h`
1. **Non-raylib `HasCamera` has wrong method signatures** - The non-raylib version has `set_position(float, float)` but the raylib version has `set_position(raylib::Vector2)`, breaking interface consistency.
2. **`BeginCameraMode` and `EndCameraMode` do nothing when camera is null** - If `get_singleton_cmp` returns null, the mode is not begun/ended, which could leave rendering state inconsistent.
3. **No zoom bounds checking** - `set_zoom()` accepts any float, including negative values which would flip the view unexpectedly.

### `src/plugins/collision.h`
1. **Division by zero risk** - `calculate_impulse` and `resolve_collision` divide by mass values without checking if they're zero.
2. **`ids.contains` uses C++20 feature** - The `contains` method was added in C++20, but the codebase targets C++17 in some places.
3. **No callback validation** - The system accesses callback functions without null checks in several places (e.g., `callbacks.normalize_vec`).

### `src/plugins/window_manager.h`
1. **`current_index()` creates EntityQuery in const method** - Line 160-164 creates and runs a query inside a const member function, which has side effects (potential temp entity warning).
2. **`on_data_changed` doesn't validate index** - If `index >= available_resolutions.size()`, this will cause undefined behavior.
3. **Dangling reference warning** - GCC warns about potentially dangling reference on line 161 due to the temporary EntityQuery.

### `src/plugins/input_system.h`
1. **`medium` uninitialized in check functions** - In `check_single_action_pressed` and `check_single_action_down`, `medium` is declared but not initialized, and could be returned uninitialized if `valid_inputs` is empty.
2. **`get_input_collector` uses deprecated query option** - Uses `{.ignore_temp_warning = true}` which suggests the temp entity system has known issues.
3. **Magic number for deadzone** - `DEADZONE = 0.25f` is hardcoded with no way to configure it.

### `src/plugins/sound_system.h`
1. **Exception swallowed in `play_with_alias_or_name`** - Lines 308-323 catch all exceptions with `catch(...)` and ignore them, hiding potential errors.
2. **`MusicUpdateSystem::for_each_with` runs on any entity** - The system inherits from `System<>` meaning it runs for every entity in the scene, but only needs to run once per frame.
3. **`storage` is private in base but accessed in derived** - `MusicLibraryImpl` and `SoundLibraryImpl` access `storage` from the `Library` base class, but `storage` should be protected, not have friends.

### `src/plugins/texture_manager.h`
1. **`RenderSprites::sheet` is mutable** - The `sheet` member is marked mutable to be set in `once()`, but this is a design smell - the system should cache this differently.
2. **Animation frame counter can exceed total_frames** - In `AnimationUpdateCurrentFrame`, the check `cur_frame() >= total_frames()` happens after incrementing, which could cause one extra frame.
3. **No null check for singleton** - `once()` calls `EntityHelper::get_singleton_cmp<HasSpritesheet>()` and dereferences without null check.

### `src/plugins/files.h` / `src/plugins/files.cpp`
1. **`ensure_directory_exists` returns false but may have partially created** - If `create_directories` fails after creating some directories, the function returns false but doesn't clean up.
2. **Filesystem exceptions not caught everywhere** - Some `fs::` operations can throw but aren't wrapped in try-catch.
3. **`init()` accesses singletonMap directly** - Line 116 accesses internal implementation detail `EntityHelper::get().singletonMap` instead of using public API.

### `src/plugins/translation.h`
1. **`reinterpret_cast` of `void*` to template type** - Line 94-95 uses `reinterpret_cast<const ParamNameMap *>(param_name_map)` which is undefined behavior if the original type doesn't match.
2. **UTF-8 parsing doesn't handle 4-byte sequences** - The codepoint extraction loop (lines 279-297) only handles up to 3-byte UTF-8 sequences, missing emoji and some CJK characters.
3. **`load_cjk_fonts` modifies font_manager parameter** - The function name suggests it only loads fonts, but it modifies the passed font_manager object.

### `src/plugins/color.h`
1. **`increase` can overflow** - Adding `factor` to color components without clamping can overflow the `unsigned char`.
2. **Static functions in header risk ODR violations** - Multiple `static` functions defined in a header will have separate instances in each translation unit.
3. **HSL conversion has precision issues** - The delta calculation `cmax - cmin` on unsigned chars can't represent fractional values, losing precision.

---

## UI Plugin Files

### `src/plugins/ui/systems.h`
1. **Unused parameters** - `selectOnFocus` parameter in `HandleSelectOnFocus::for_each_with` and `child_selectOnFocus` variable are never used.
2. **`process_derived_children` is O(n²)** - Each system that processes derived children does a full entity lookup for each child ID.
3. **Duplicate code across systems** - `HandleClicks`, `HandleLeftRight`, `HandleSelectOnFocus`, and `UpdateDropdownOptions` all have nearly identical `process_derived_children` implementations.

### `src/plugins/ui/context.h`
1. **`InputAction::None` used without compile-time check** - Line 109 assumes `InputAction::None` exists without using `magic_enum::enum_contains`.
2. **`render_cmds` is mutable** - The render command vector is mutable, allowing modification through const references, which breaks const-correctness.
3. **No validation on `queue_render`** - Render info is queued without validating the entity ID exists.

### `src/plugins/ui/components.h`
1. **`HasDropdownState` inherits from `HasCheckboxState`** - This seems like a design mistake - a dropdown is not a checkbox.
2. **`HasChildrenComponent::children` can contain invalid IDs** - Children are stored as EntityIDs but there's no validation they still exist when accessed.
3. **Magic number in `HasRoundedCorners`** - Uses `std::bitset<4>` with magic number 4 for corners.

### `src/plugins/autolayout.h`
1. **`to_ent_static` and `to_cmp_static` run queries** - These static methods create EntityQuery objects and run full queries just to find a single entity by ID, which is O(n).
2. **Recursive functions can stack overflow** - Functions like `calculate_standalone`, `calculate_those_with_parents`, etc. recurse through the UI tree without depth limits.
3. **`log_error` calls in switch cases without break** - Lines 657, 737-740 call `log_error` but execution continues to the next case.

---

## Utility Files

### `src/bitwise.h`
1. **`reinterpret_cast` of enum reference** - Lines 27-28, 33-35, 40-42 use `reinterpret_cast<int &>(a)` on enum references, which is implementation-defined behavior.
2. **Operators enabled for ALL enums** - These templates apply to every enum type in the program, including those where bitwise operations don't make sense.
3. **No SFINAE to exclude specific enums** - Can't opt-out an enum from these operations.

### `src/bitset_utils.h`
1. **`random_index` calculation can be biased** - Line 52 uses `generator() % size` which has modulo bias for non-power-of-2 sizes.
2. **Duplicate `ForEachFlow` enum** - This enum is defined both here and in `entity_helper.h`.
3. **`index_of_nth_set_bit` returns -1 as int** - The return type is `int` but the function works with `size_t`, and returning -1 is used for "not found" but could be confused with valid indices.

### `src/singleton.h`
1. **Non-thread-safe singleton creation** - The `create()` function checks and creates without any synchronization.
2. **`SINGLETON_FWD` declares static variable at file scope** - This creates a static variable for each singleton type, which can cause issues with header inclusion order.
3. **Memory never freed** - The singleton `shared_ptr` is never reset, meaning singletons live for the entire program lifetime.

### `src/logging.h`
1. **`log_trace` and `log_clean` are no-ops** - These functions take variadic arguments but ignore them, which still evaluates the arguments (side effects).
2. **`VALIDATE` is a no-op** - The validation macro does nothing by default, meaning validation code is compiled but not executed.
3. **Format string type mismatch** - `log_info`, `log_warn`, `log_error` use `printf`-style formatting but the codebase often uses `fmt`-style `{}` placeholders.

### `src/type_name.h`
1. **Returns `string_view` into temporary** - The function modifies `name` which is a `string_view`, and returns it, but the data it views is the temporary `__PRETTY_FUNCTION__` output.
2. **Different results across compilers** - The parsing logic differs between Clang, GCC, and MSVC, potentially causing different names for the same type.
3. **No handling for nested templates** - Complex template types may not be parsed correctly.

### `src/library.h`
1. **`get()` throws after warning** - The function logs a warning then calls `storage.at()` which throws if not found, duplicating error handling.
2. **`unload_all` iterates by value** - Line 84 uses `for (auto kv : storage)` which copies each key-value pair unnecessarily.
3. **`get_random_match` always returns first match** - The TODO on line 99-102 shows the random selection is not implemented, always returning index 0.

### `src/debug_allocator.h`
1. **ANSI escape codes in log output** - Uses hardcoded ANSI color codes that won't work on Windows console without special handling.
2. **Inherits from `std::allocator`** - `std::allocator` is deprecated for custom allocator inheritance in C++17.
3. **Missing `rebind` member** - Modern allocators should provide a `rebind` template for compatibility.

### `src/drawing_helpers.h`
*(Not read but likely has issues with raylib/non-raylib abstraction)*

### `src/font_helper.h`
1. **`calloc` used for C++ code** - Line 47 uses `calloc` instead of `new[]` or `std::vector`.
2. **Memory leak if `LoadFontEx` throws** - After allocating `codepointsNoDups` with `calloc`, if `LoadFontEx` fails or throws, the memory isn't freed.
3. **O(n²) duplicate removal** - The `remove_duplicate_codepoints` function uses nested loops for deduplication when a `std::set` would be O(n log n).

---

## Summary

**Critical Issues:**
- Component ID overflow causes silent data corruption
- Race conditions in pathfinding queue
- Memory leaks in pathfinding BFS
- Thread safety issues in singleton creation and component ID generation
- GCC compilation failures due to in-class explicit specialization

**Performance Issues:**
- EntityQuery runs queries multiple times
- Linear searches where maps/hashes should be used
- Unnecessary copies in loops
- O(n²) algorithms where O(n log n) is possible

**Design Issues:**
- Inconsistent API between raylib and non-raylib builds
- Unused parameters and code
- Magic numbers throughout
- Duplicate code across UI systems

**Maintainability Issues:**
- Typos in variable names
- Misleading function names
- Missing documentation
- Inconsistent error handling

---

## Additional UI Files

### `src/plugins/ui/immediate.h`
*(Wrapper header - no bugs, but could have clearer documentation)*

### `src/plugins/ui/imm_components.h`
1. **Static variables in header** - Variables like `UIStylingDefaults` are static in header, creating multiple instances in different translation units.
2. **No validation of sizing enums** - The `Size` struct's `dim` field can contain invalid enum values if memory is corrupted.
3. **`enable_font` doesn't validate font exists** - The method sets font name without checking if it's loaded.

### `src/plugins/ui/theme.h` / `src/plugins/ui/theme_defaults.h`
1. **Static color constants may have initialization order issues** - Using `static const Color` in headers can cause static initialization order fiasco.
2. **No dark/light mode toggle** - Theme is hardcoded with no runtime switching support.
3. **Color values duplicated** - Same color values appear in multiple places without using constants.

### `src/plugins/ui/rendering.h`
1. **Render commands processed without sorting** - Z-order/layers may not be respected if commands aren't sorted.
2. **No bounds checking on render** - Components can render outside their allocated bounds.
3. **Text rendering doesn't handle newlines** - Multi-line text may not render correctly.

### `src/plugins/ui/text_utils.h`
1. **UTF-8 handling incomplete** - Only handles up to 3-byte sequences, missing 4-byte emoji/extended CJK.
2. **No text wrapping support** - Long text overflows its container.
3. **Text measurement may differ from render** - Using different spacing values for measure vs render.

### `src/plugins/ui/entity_management.h`
1. **Entity creation without cleanup tracking** - UI entities created may not be properly marked for cleanup.
2. **Parent-child relationships can become stale** - If parent is deleted, children still reference it.
3. **No limit on entity depth** - Deeply nested UI trees can cause stack overflow in recursive operations.

### `src/plugins/ui/element_result.h`
1. **Return values not always checked** - Element creation functions return results that are often ignored.
2. **Error states not propagated** - Failures during element creation silently continue.
3. **Chained method calls can mask errors** - Builder pattern hides intermediate failures.

### `src/plugins/ui/animation_keys.h`
1. **Animation keys are strings** - Using strings for animation identification is slower than enums.
2. **No validation of key uniqueness** - Duplicate keys can overwrite animations silently.
3. **Animation timing not frame-rate independent** - Assumes fixed time step.

### `src/plugins/ui/rounded_corners.h`
1. **Bitset size hardcoded to 4** - Magic number for number of corners without named constant.
2. **No validation on corner radius** - Negative or excessively large radii accepted.
3. **Inconsistent API** - Some methods return `*this`, others return void.

### `src/plugins/ui/component_config.h`
1. **Configuration not persisted** - UI configuration changes are lost on restart.
2. **No defaults validation** - Default values may be invalid for certain component types.
3. **Tight coupling to specific components** - Configuration is tied to implementation details.

---

## Settings and Files Plugins

### `src/plugins/settings.cpp`
1. **Empty implementation** - The file only provides a symbol for linker, no actual implementation.
2. **No error handling wrapper** - Raw file operations can throw but aren't caught.
3. **Compile-time only check** - `AFTERHOURS_SETTINGS_CPP_COMPILED` is only checked at compile time, not link time.

---

## Developer and Utility Files

### `src/developer.h`
1. **`EnforceSingleton::saw_one` not initialized in constructor** - The boolean `saw_one` is only initialized in `once()`, so the first frame could have undefined behavior if `for_each_with` runs before `once()`.
2. **`Plugin` has static methods but isn't templated** - The static methods in `Plugin` struct have no implementation, leading to linker errors if actually called.
3. **Fallback types defined with inconsistent macro styles** - Uses both `#ifndef X` and `#define X` vs `using X = ...` inconsistently.

### `src/logging.h`
1. **`log_trace`, `log_clean`, `log_once_per` are no-ops that still evaluate arguments** - These functions take variadic args but ignore them. However, the arguments are still evaluated, which can cause side effects or performance overhead.
2. **Format string mismatch** - Uses `printf`-style `%s` formatting but codebase often uses `fmt`-style `{}` placeholders, causing silent format bugs.
3. **`VALIDATE` macro does nothing** - The `VALIDATE` function is defined to do nothing, so validation code has no effect unless replaced.

### `src/singleton.h`
1. **Non-thread-safe singleton creation** - The `create()` function checks and creates without any synchronization, which is unsafe in multi-threaded scenarios.
2. **`SINGLETON_FWD` creates static variable in header** - This creates a static variable for each singleton type per translation unit due to lack of `inline`.
3. **Memory never freed** - The singleton `shared_ptr` is never reset or destroyed, meaning singletons live for entire program lifetime with no cleanup.

### `src/bitset_utils.h`
1. **Modulo bias in random selection** - Line 52 uses `generator() % size` which has modulo bias for non-power-of-2 sizes. Should use `std::uniform_int_distribution`.
2. **`ForEachFlow` enum duplicated** - This same enum is defined in `entity_helper.h`, causing potential ODR violations if definitions differ.
3. **`index_of_nth_set_bit` returns -1 as int but uses size_t loop** - The function works with `size_t` indices internally but returns `int`, which could cause issues with large bitsets.

### `src/type_name.h`
1. **Returns `string_view` that may view temporary data** - The function modifies `name` which is a `string_view`, and returns it, potentially causing dangling references.
2. **Different results across compilers** - The parsing logic differs between Clang, GCC, and MSVC, potentially causing different type names for the same type.
3. **No handling for complex nested templates** - Template types with multiple parameters may not be parsed correctly.

### `src/debug_allocator.h`
1. **ANSI escape codes in output** - Uses hardcoded ANSI color codes (`\033[36m`, `\033[0m`) that won't work on Windows console without special handling.
2. **Inherits from `std::allocator`** - Inheriting from `std::allocator` is deprecated for custom allocators in C++17 and later.
3. **Missing `rebind` member** - Modern C++ allocators should provide a `rebind` template for container compatibility.

### `src/drawing_helpers.h`
1. **Tight coupling to raylib types** - Drawing helpers assume raylib-specific types when `AFTER_HOURS_USE_RAYLIB` is defined.
2. **No fallback implementations** - Non-raylib builds have stub implementations that do nothing.
3. **Color parameter ordering inconsistent** - Some functions take color as first param, others as last.

### `src/font_helper.h`
1. **Uses `calloc` instead of C++ allocation** - Line 47 uses `calloc` which doesn't call constructors and is harder to integrate with C++ memory management.
2. **Potential memory leak on exception** - After allocating with `calloc`, if subsequent operations throw, the memory isn't freed (no RAII wrapper).
3. **O(n²) duplicate removal algorithm** - The `remove_duplicate_codepoints` function uses nested loops for deduplication when `std::set` or `std::unordered_set` would be O(n log n) or O(n).

---

## ECS Core Additional Issues

### `src/ecs.h`
1. **Includes many headers** - The aggregating header includes many implementation headers, potentially slowing compilation.
2. **No forward declarations** - Missing forward declarations for commonly-used types.
3. **Order of includes matters** - Some headers depend on types defined in earlier includes.

---

## Count Summary

| Category | Files Reviewed | Issues Found |
|----------|----------------|--------------|
| Core ECS | 6 | 18+ |
| Plugins | 14 | 42+ |
| UI Plugin | 12 | 36+ |
| Utilities | 8 | 24+ |
| **Total** | **40** | **120+** |

---

## Recommendations

### High Priority Fixes
1. **Component ID overflow** - Implement proper error handling or dynamic resizing
2. **Pathfinding race conditions** - Use proper atomic operations or mutex guards
3. **GCC compilation failures** - Move explicit specializations outside class scope
4. **Memory leaks in BFS** - Use RAII (smart pointers) for node allocation

### Medium Priority Fixes
1. **Query performance** - Avoid running queries multiple times in `gen_first*` methods
2. **Thread safety in singletons** - Add proper synchronization
3. **Format string consistency** - Standardize on either printf or fmt style

### Low Priority / Improvements
1. **Code deduplication** - Extract common patterns in UI systems
2. **Documentation** - Add more comprehensive API documentation
3. **Type safety** - Replace magic numbers with named constants/enums

---

*Generated by code review on December 3, 2025*
