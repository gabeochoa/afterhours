# Animation Plugin

1. **HIGH — Use a contiguous array of active tracks instead of iterating all tracks in the hash map.**
   `AnimationManager::update()` iterates every track, including inactive ones. Maintain a separate list of active track keys for O(active) instead of O(total).

2. **HIGH — Replace `std::unordered_map<Key, AnimTrack>` with a flat array when keys are enum-based.**
   Most animation keys are small enums. A `std::array<AnimTrack, enum_count>` gives O(1) access with zero hashing overhead.

3. **MED — Avoid `std::function<void()>` for `on_complete` callbacks; use a lighter callable.**
   `std::function` heap-allocates for non-trivial callables. Use a function pointer + void* context, or `std::move_only_function`.

4. **MED — Skip watcher notification when the track value hasn't changed (epsilon check).**
   `update()` always iterates all watchers and calls `quantize()`. Skip when `new_current == tr.current`.

5. **MED — Use `std::deque` replacement (ring buffer) for `AnimTrack::queue`.**
   `std::deque` has per-chunk allocation. A small ring buffer (most sequences are < 8 segments) avoids heap allocations.

6. **MED — Pool-allocate `AnimSegment` queue entries.**

7. **MED — Avoid `std::lerp` call; inline as `from + (to - from) * u`.**
   `std::lerp` may have NaN/infinity handling overhead depending on implementation.

8. **MED — Remove the `static AnimationManager<Key> m` pattern; make it an explicit singleton.**
   Function-local statics have a hidden mutex guard on first access (C++11 thread-safe init).

9. **MED — Use `static std::unordered_set<Key>` in `one_shot` — this leaks memory. Use a component flag instead.**

10. **MED — Batch animation updates per type to improve cache locality.**

11. **LOW — Avoid `std::optional<float>` in `get_value`; use a sentinel value (NaN).**

12. **LOW — Use `std::clamp` only in `apply_ease`; trust the caller for pre-clamped `t` values.**

13. **LOW — Inline `apply_ease` for the common `Linear` case with a fast-path branch.**

14. **LOW — Pre-compute `1.0f / tr.duration` to replace division with multiplication.**

15. **LOW — Use SIMD for batch-updating multiple tracks of the same type.**

16. **LOW — Avoid `ensure_track()` in chained API calls (from/to/sequence); resolve once.**

17. **LOW — Use `std::move` for `AnimSegment` when popping from the queue.**

18. **LOW — Reduce `CompositeKeyHash` cost with a simpler hash for small indices.**

19. **LOW — Mark `apply_ease` as `constexpr` for compile-time evaluation of constant tracks.**

20. **LOW — Add a `dirty` flag to `AnimationManager` to skip `update()` when nothing is active.**

21. **LOW — Avoid allocating `std::vector<Watcher>` when no watchers are used (common case).**

22. **LOW — Use `SmallVector<Watcher, 1>` since most tracks have 0-1 watchers.**

23. **LOW — Pre-sort active tracks by completion time for early-out.**

24. **LOW — Collapse sequential segments with the same easing into a single longer segment.**

25. **LOW — Use `float` comparison with epsilon instead of `>=` for completion check.**

26. **LOW — Profile `CompositeKey` hash distribution and tune the magic constant.**

27. **LOW — Move `AnimHandle` methods into the `.cpp` to reduce header bloat and compile times.**

28. **LOW — Use `reserve()` on the watcher vector based on typical usage patterns.**

29. **LOW — Consider ECS-based animation: store `AnimTrack` as a component, iterate with a system.**

30. **LOW — Avoid `std::function<int(float)>` for quantize; use a template parameter.**

31. **LOW — Use `[[nodiscard]]` on `is_active()` and `value()` to catch unused results.**

32. **LOW — Pre-compute easing lookup tables for non-linear easing types.**

33. **LOW — Use `constinit` for the static `AnimationManager` to avoid lazy initialization.**

34. **LOW — Consider separating active/inactive tracks into two pools for better iteration.**

35. **LOW — Add branch prediction hints for the `!tr.active` early exit in `update()`.**
