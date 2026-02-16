# Input Plugin

1. **HIGH — Replace `EntityQuery` in `get_input_collector()` with singleton lookup.**
   Comment says "TODO replace with a singleton query". This runs every frame in UI code and does a full entity scan. Use `EntityHelper::get_singleton_cmp<InputCollector>()` directly.

2. **HIGH — Cache `get_mouse_position()` result once per frame.**
   The resolution scaling math (aspect ratio, letterboxing) is recalculated every call. Cache the result.

3. **MED — Replace `std::map<int, ValidInputs>` with a flat array or `std::vector`.**
   Action IDs are typically small integers. A vector indexed by action ID is O(1) with no hashing.

4. **MED — Avoid `std::variant` visitor overhead in hot input checking loops.**
   `check_single_action_pressed` uses `input.index()` checks (effectively a switch on variant index). Direct type dispatch or separate arrays per input type would be faster.

5. **MED — Skip gamepad polling when no gamepads are connected.**
   `fetch_max_gampad_id` scans all 8 possible gamepads every frame. Cache the result and only rescan periodically.

6. **MED — Avoid creating `ValidInputs` copies in the input loop.**
   `const ValidInputs vis = kv.second;` copies the vector every iteration. Use `const auto& vis = kv.second;`.

7. **MED — Use bitfields for action state (down/pressed/released) instead of vectors of `ActionDone`.**
   `collector.inputs` and `collector.inputs_pressed` are vectors that grow/shrink every frame. A fixed bitfield per action would be O(1) with no allocation.

8. **MED — Pre-compute `inputs_as_bits` once instead of per-call in UI systems.**

9. **MED — Replace `magic_enum::enum_index` calls in hot paths with a pre-computed lookup table.**
   `magic_enum` operations can be surprisingly expensive.

10. **MED — Avoid redundant `is_gamepad_available` calls; cache per frame.**

11. **LOW — Use `std::array` instead of `std::vector` for `ValidInputs` when size is known at compile time.**

12. **LOW — Mark `visit_key`, `visit_axis`, etc. as `[[gnu::always_inline]]`.**

13. **LOW — Avoid `abs()` call on gamepad axis; use `fabsf()` directly.**

14. **LOW — Pool-allocate `ActionDone` objects or use a ring buffer.**

15. **LOW — Replace `std::string` returns in `name_for_button` / `icon_for_button` with `string_view`.**
    These allocate a new `std::string` for every call.

16. **LOW — Avoid `std::to_string` in `RenderConnectedGamepads`; use `fmt::format_to` with a stack buffer.**

17. **LOW — Cache the `ProvidesCurrentResolution` singleton lookup in `get_mouse_position`.**

18. **LOW — Use `__builtin_expect` for the common case of no test mode in `#ifdef AFTER_HOURS_ENABLE_E2E_TESTING`.**

19. **LOW — Avoid `std::pair<DeviceMedium, float>` return; use output parameters or a struct.**

20. **LOW — Pre-compute `static_cast<InputAction>(actions_done.action)` only when needed.**

21. **LOW — Avoid iterating all mappings when only a subset of actions are typically active.**

22. **LOW — Use SIMD to check multiple gamepad axes simultaneously.**

23. **LOW — Avoid float comparisons (`amount > 0.f`); use integer flags.**

24. **LOW — Hoist the `while (i <= mxGamepadID)` loop outside the mapping loop when only keyboard is connected.**

25. **LOW — Use `constexpr` for DEADZONE value (already done) and propagate to compile-time checks.**

26. **LOW — Flatten the `ProvidesLayeredInputMapping` nested maps into a single contiguous array.**

27. **LOW — Avoid `collector.inputs.push_back()` in the hot loop; pre-allocate to max possible size.**

28. **LOW — Use a `static thread_local` buffer for the input collector instead of heap-allocated vectors.**

29. **LOW — Mark the `struct GamepadButton` constructor as `constexpr`.**

30. **LOW — Deduplicate the raylib / metal / stub code paths with a single abstraction layer.**

31. **LOW — Use `std::round` instead of manual rounding in mouse position calculation.**

32. **LOW — Avoid the `std::optional<std::reference_wrapper<InputCollector>>` wrapper; use a raw pointer.**

33. **LOW — Cache `content_w / content_h` ratio as a float instead of recomputing per-call.**

34. **LOW — Precompute `scale_x` and `scale_y` once per resolution change, not per `get_mouse_position()`.**

35. **LOW — Use `int` math for letterbox computation instead of `float` -> `int` round-trips.**
