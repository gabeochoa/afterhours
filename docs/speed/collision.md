# Collision Plugin

1. **HIGH — Replace O(N^2) broad-phase with spatial hashing or a grid.**
   `for_each_with` runs an `EntityQuery` for every entity, making collision detection O(N^2). A spatial hash grid reduces this to O(N) for uniformly distributed entities.

2. **HIGH — Remove per-entity `EntityQuery` allocation inside `for_each_with`.**
   Each entity constructs a full query, materializes a `RefEntities` vector, and scans all entities. This is the single biggest bottleneck.

3. **HIGH — Cache the collidable entity list once per frame in `once()` instead of per-entity.**
   Move the `EntityQuery().whereHasComponent<Transform>()` call into `once()` and store the result.

4. **HIGH — Use `std::unordered_set<int>` instead of `std::set<int>` for `ids`.**
   `std::set` is a red-black tree with O(log N) lookups per `contains()` call. Hot path.

5. **MED — Replace `std::function` callbacks with template parameters or function pointers.**
   The `Callbacks` struct contains 8 `std::function` members, each invoked per collision pair. Virtual dispatch overhead per-pair.

6. **MED — Inline `normalize_vec`, `dot_product`, `vector_length` instead of going through `std::function`.**
   These are simple math operations that should be inlined, not called through function pointers.

7. **MED — Compute `1.0f / mass` once per entity, not per collision pair.**
   `inv_mass_a` and `inv_mass_b` are recomputed multiple times in `resolve_collision`. Cache on the transform.

8. **MED — Skip collision pairs where both masses are infinite.**
   If both entities have `mass == numeric_limits<float>::max()`, no resolution is needed.

9. **MED — Use squared distance for overlap checks to avoid `sqrt`.**
   `calculate_penetration_depth` uses `std::min(overlapX, overlapY)` which is fine, but `callbacks.vector_length` likely calls `sqrt`. Use squared comparisons where possible.

10. **MED — SIMD-vectorize the impulse and friction calculations.**
    The vec2 math in `resolve_collision` is simple enough for SSE/NEON intrinsics.

11. **MED — Sort entities by position for better spatial locality during collision checks.**

12. **MED — Early-out in `resolve_collision` if relative velocity is zero.**

13. **MED — Skip `calculate_dynamic_restitution` when speed is low (micro-optimization).**

14. **MED — Avoid calling `config.get_collision_scalar()` and `config.get_max_speed()` per pair; cache per frame.**

15. **MED — Use `reserve()` on the `ids` set or switch to a flat bitset indexed by entity ID.**

16. **LOW — Avoid `std::max`, `std::min`, `std::clamp` function call overhead; use intrinsics.**

17. **LOW — Align collision config data for SIMD access.**

18. **LOW — Batch collision resolution: detect all pairs first, resolve all at once.**

19. **LOW — Use sweep-and-prune on one axis as a simple broad-phase.**

20. **LOW — Profile `callbacks.check_overlap` — if it's AABB, inline it directly.**

21. **LOW — Consider fixed-point math for collision if floating-point precision isn't needed.**

22. **LOW — Use `std::bit_cast` for fast float-to-int conversion in spatial hashing.**

23. **LOW — Remove the `callbacks.should_skip_entity` check from the inner loop; filter in `once()`.**

24. **LOW — Remove the `callbacks.is_floor_overlay` check from the inner loop; use a tag/bitset.**

25. **LOW — Remove the `callbacks.gets_absorbed` check from the inner loop; use a tag/bitset.**

26. **LOW — Pool-allocate collision pairs for batch processing.**

27. **LOW — Use `constexpr` for collision constants (friction coefficients, etc.).**

28. **LOW — Avoid re-reading `transform` from entity in inner loop; pass by reference from outer.**

29. **LOW — Profile branch misprediction on the mass checks and reorder conditions.**

30. **LOW — Consider a contact cache for persistent collision pairs (warm starting).**

31. **LOW — Use `__restrict__` pointers for transform references to help the compiler.**

32. **LOW — Minimize `std::optional` usage in `get_absorber_parent_id`; use sentinel value.**

33. **LOW — Pre-compute `std::sqrt(a.friction * b.friction)` as a lookup table for common values.**

34. **LOW — Use `float` instead of `double` consistently (avoid implicit promotions).**

35. **LOW — Mark collision math functions `[[gnu::always_inline]]` or `__forceinline`.**
