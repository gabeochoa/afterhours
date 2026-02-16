# Timer Plugin

1. **MED — Skip `TimerUpdateSystem` iteration for entities whose timer is already expired and not auto-reset.**

2. **MED — Use a dirty flag to skip timer update when paused.**

3. **MED — Batch all timer updates into a single SIMD pass.**

4. **MED — Use a sorted priority queue for timer expiration instead of checking all timers every frame.**

5. **MED — Combine `TimerUpdateSystem` and `TriggerUpdateSystem` into a single pass.**

6. **LOW — Avoid virtual dispatch for `for_each_with` in timer systems; use CRTP.**

7. **LOW — Use `float` accumulator with epsilon for timer comparison instead of `>=`.**

8. **LOW — Pre-compute `1.0f / reset_time` for progress calculation.**

9. **LOW — Avoid `std::max(0.0f, ...)` in `get_remaining()`; use branchless max.**

10. **LOW — Use `[[likely]]` on the `!paused` branch.**

11. **LOW — Inline `HasTimer::update()` since it's a trivial function.**

12. **LOW — Avoid `bool` return from `update()` when not used; use a void overload.**

13. **LOW — Pack `HasTimer` fields to minimize struct size.**

14. **LOW — Use `alignas(16)` for timer component arrays for SIMD.**

15. **LOW — Avoid iterating entities that only have expired, non-auto-reset timers.**

16. **LOW — Consider event-based timer expiration instead of per-frame polling.**

17. **LOW — Profile branch misprediction on `is_ready()` checks.**

18. **LOW — Use `constexpr` default values for timer fields.**

19. **LOW — Consider grouping timers by expiration time for batch processing.**

20. **LOW — Avoid per-entity virtual dispatch; iterate the component array directly.**

21. **LOW — Mark `is_ready()`, `get_progress()`, etc. as `[[gnu::pure]]`.**

22. **LOW — Use a timer wheel for large numbers of timers with different durations.**

23. **LOW — Pre-allocate timer component storage based on expected entity count.**

24. **LOW — Avoid `is_path_empty()` calling `path.empty()` on deque; use cached `path_size`.**

25. **LOW — Use `static_assert` to verify `HasTimer` is trivially copyable for fast moves.**

26. **LOW — Profile and tune the fixed tick rate for timer precision vs. performance.**

27. **LOW — Skip `TriggerOnDt::test()` when `current` is far from `reset`.**

28. **LOW — Use monotonic clock for timer accuracy instead of accumulating `dt` errors.**

29. **LOW — Consider making timers a global array instead of per-entity components.**

30. **LOW — Profile memory layout of `HasTimer` and consider SoA for batch updates.**

31. **LOW — Avoid `float` division in `get_progress()`; multiply by pre-computed reciprocal.**

32. **LOW — Cache `dt` in a register for the entire system pass.**

33. **LOW — Use `__builtin_expect` for the rare `reset_time <= 0.0f` case.**

34. **LOW — Consider using `double` for timer accumulators to avoid precision loss over long durations.**

35. **LOW — Coalesce adjacent timers in memory for prefetcher-friendly access patterns.**
