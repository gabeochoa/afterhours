# Afterhours Performance Optimization Brainstorm

Comprehensive catalogue of speed-up opportunities across the core ECS and every plugin.
Each item is tagged with estimated impact (**HIGH** / **MED** / **LOW**) and rough effort.

---

## Table of Contents

1. [Core ECS (50 items)](#core-ecs)
2. [Collision Plugin (35 items)](#collision-plugin)
3. [Pathfinding Plugin (35 items)](#pathfinding-plugin)
4. [Input Plugin (35 items)](#input-plugin)
5. [Animation Plugin (35 items)](#animation-plugin)
6. [UI / AutoLayout Plugin (35 items)](#ui--autolayout-plugin)
7. [Sound System Plugin (35 items)](#sound-system-plugin)
8. [Texture Manager Plugin (35 items)](#texture-manager-plugin)
9. [Timer Plugin (35 items)](#timer-plugin)
10. [Camera Plugin (35 items)](#camera-plugin)

---

## Core ECS

### Entity Storage & Memory Layout

1. **HIGH — Replace `std::shared_ptr<Entity>` with a slot-map / arena allocator.**
   `shared_ptr` has atomic ref-count overhead on every copy/move. A pool allocator with stable indices removes the per-entity heap allocation and refcount traffic entirely. Every system iteration currently pays for pointer indirection + cache miss.

2. **HIGH — Store components in Structure-of-Arrays (SoA) instead of per-Entity `ComponentArray`.**
   Currently each `Entity` owns a `std::array<unique_ptr<BaseComponent>, 128>`. This means iterating over a single component type touches memory scattered across every entity object. SoA column stores (one contiguous vector per component type) would give perfect cache locality for system iteration.

3. **HIGH — Remove virtual destructor / `BaseComponent` inheritance; use type-erased flat storage.**
   Every component goes through `std::unique_ptr<BaseComponent>` which forces a heap allocation and a vtable pointer per component. Type-erased in-place storage (like `entt::basic_storage`) eliminates the vtable, the heap alloc, and the pointer indirection.

4. **HIGH — Replace `componentArray` fixed-size 128-entry array with sparse set or archetype table.**
   Each entity wastes `128 * sizeof(unique_ptr)` = 1 KiB regardless of how many components it has. Archetype storage groups entities with the same component signature, giving dense iteration and no wasted space.

5. **HIGH — Eliminate `std::atomic_int ENTITY_ID_GEN` contention for single-threaded paths.**
   Atomic increment has a full memory fence on x86. For the common single-threaded case, use a plain `int` with a compile-time switch.

6. **HIGH — Avoid `merge_entity_arrays()` every system tick.**
   `SystemManager::tick()` calls `EntityHelper::merge_entity_arrays()` after every single update system. This is O(N) per system per frame. Batch all entity creation and do a single merge at the end of the frame.

7. **HIGH — Use bitset intersection for system component matching instead of per-entity virtual calls.**
   `HasAllComponents::value()` does a fold expression that calls `entity.has<T>()` for each type. Pre-compute the system's required `ComponentBitSet` once and test with `(entity.componentSet & required) == required` — a single bitwise AND + compare.

8. **MED — Reserve entity vectors to expected capacity.**
   `entities_DO_NOT_USE` and `temp_entities` grow via amortized doubling. If the game knows roughly how many entities it will have, `.reserve()` up front avoids reallocations and copies of `shared_ptr` arrays.

9. **MED — Use `std::vector<Entity>` with stable handles instead of `vector<shared_ptr<Entity>>`.**
   Removes a layer of pointer indirection. Entities live contiguously in memory. Combined with the slot-map handle system you already have, this is straightforward.

10. **MED — Pool-allocate entities instead of `std::make_shared<Entity>()` per creation.**
    Custom allocator + `std::allocate_shared` or a simple free-list reduces allocation pressure.

11. **MED — Shrink `ComponentBitSet` / `ComponentArray` to actual max used.**
    `AFTER_HOURS_MAX_COMPONENTS` defaults to 128. Most games use 30-50. Shrinking to 64 halves the bitset and array size, improving cache usage.

12. **MED — Replace `std::set<int> permanant_ids` with `std::unordered_set<int>` or a bitset.**
    `std::set` is a red-black tree with O(log N) lookups and poor cache behaviour. An `unordered_set` or flat bitset is O(1).

13. **MED — Use swap-and-pop for entity cleanup instead of erase-from-middle.**
    Already partially done, but the `while (i < entities.size())` loop in `cleanup()` still does `std::swap` + `pop_back` which is good — verify the singleton cleanup inner loop isn't O(N*M).

14. **MED — Make `getEntityAsSharedPtr()` O(1) via the slot map.**
    Currently does a linear scan of all entities comparing IDs. Use the `id_to_slot` mapping instead.

15. **MED — Flatten the `singletonMap` to a fixed-size array indexed by `ComponentID`.**
    `std::unordered_map<ComponentID, Entity*>` has hash overhead. Since `ComponentID` is a small integer, a `std::array<Entity*, max_num_components>` gives O(1) with zero overhead.

16. **MED — Cache `get_type_id<T>()` result in the calling system rather than re-querying.**
    The static-local initialization is fast but not free; repeated calls in hot loops add up. Store the ID in a `constexpr`/`static const` at the system level.

17. **MED — Remove `dynamic_cast` from `has_child_of()` / `get_with_child()`.**
    `dynamic_cast` is very expensive (traverses type info). Replace with a component registration system that tracks inheritance, or use a tag/trait approach.

18. **LOW — Mark `Entity(Entity&&)` as `noexcept` (already done) and ensure move is trivial.**
    Verify no hidden copies from `componentArray` move (array of 128 unique_ptrs). Consider lazy initialization.

19. **LOW — Use `[[likely]]` / `[[unlikely]]` annotations on hot paths.**
    The `if (temp_entities.capacity() == 0) [[unlikely]]` pattern is already used in one place. Apply broadly: null-entity checks, cleanup flag checks, etc.

20. **LOW — Compile with `-fno-rtti` when `has_child_of` is not used.**
    RTTI adds overhead to every polymorphic type. If `dynamic_cast` paths are unused, stripping RTTI saves space and can improve icache.

### Entity Query

21. **HIGH — Cache query results per frame for repeated identical queries.**
    Games frequently run the same query (e.g., "all entities with Transform") in multiple systems. A per-frame query cache keyed by the component bitset avoids redundant scans.

22. **HIGH — Replace `std::function<bool(const Entity&)>` filters with template predicates.**
    `std::function` has virtual dispatch + possible heap allocation. Template-based filters inline completely. The `SKIP_ENTITY_QUERY_MODIFICATIONS` path helps but still uses `std::function`.

23. **HIGH — Pre-build per-component-type entity lists (component indices).**
    Maintain a `std::vector<Entity*>` per component type. When iterating "all entities with ComponentX", iterate only that list instead of scanning all entities.

24. **MED — Avoid `out.reserve(entities.size() / 2)` guess; use component population stats.**
    Track how many entities have each component. Use that count for reserve instead of a 50% guess.

25. **MED — Remove the `shared_ptr<Modification>` wrapper in `add_mod()`.**
    Each `add_mod` creates a `shared_ptr`, wraps it in a `std::function`, and stores it. That's 2 heap allocations per filter. Use `std::unique_ptr` or inline storage.

26. **MED — Short-circuit multi-component queries with the rarest component first.**
    If querying for `A, B, C` where A exists on 1000 entities but C on 10, check C first. Track component population counts to auto-order.

27. **MED — Add a `gen_for_each(callback)` that avoids materializing a `RefEntities` vector.**
    Many callers do `for (Entity& e : query.gen()) { ... }`. This allocates a vector just to iterate it. A callback-based API or a lazy iterator avoids the allocation.

28. **MED — Pool-allocate or use small-buffer-optimization for the `mods` vector.**
    Most queries have 1-3 filters. A `SmallVector<FilterFn, 4>` avoids heap allocation for common cases.

29. **LOW — Make `EntityQuery` movable but not copyable to prevent accidental copies.**
    Already mostly the case, but the implicit copy of `std::vector<FilterFn>` can silently duplicate shared_ptrs.

30. **LOW — Replace `std::optional<OrderByFn>` with a raw function pointer + null check.**
    Avoids the overhead of `std::optional<std::function<...>>`.

### System Execution

31. **HIGH — Skip systems whose required components have zero entities.**
    Before iterating all entities, check the component population count. If no entity has the required component, skip the entire system.

32. **HIGH — Avoid iterating ALL entities for every system.**
    `SystemManager::tick()` iterates every entity for every system. With component indices (item 23), each system only sees entities it cares about.

33. **HIGH — Remove double iteration in `render()` (non-const then const pass).**
    `render()` iterates all entities twice (mutable pass, then const pass) unless `AFTERHOURS_SINGLE_RENDER_PASS` is defined. Default to single pass.

34. **MED — Parallelize independent systems with a task graph.**
    Systems that don't share mutable components can run concurrently. Build a dependency graph at registration time and execute independent systems in parallel.

35. **MED — Use `std::execution::par_unseq` for entity iteration within a system.**
    For systems that are read-only or have no cross-entity dependencies, parallel iteration over the entity list can use all cores.

36. **MED — Batch `merge_entity_arrays()` calls.**
    Currently called after every update system. Call once after all update systems, once after all fixed-update systems, etc.

37. **MED — Fixed tick: avoid repeated `fixed_tick()` calls in a loop; batch N ticks.**
    `fixed_tick_all()` loops `while (num_ticks > 0)` calling `fixed_tick()` each time. If systems are stateless, batch the dt.

38. **MED — Pre-sort entities by archetype so system iteration is naturally grouped.**
    If entities with the same component set are contiguous in memory, iteration skips fewer entities.

39. **LOW — Inline `should_run()` / `once()` / `after()` checks.**
    These are virtual calls with trivial default implementations. Use CRTP or `if constexpr` to eliminate the vtable lookup when unused.

40. **LOW — Avoid calling `for_each_derived` path unless `include_derived_children` is true.**
    Already gated by the flag, but the branch is inside the hot entity loop. Hoist it outside.

### Handle & Slot System

41. **MED — Pre-allocate slots to expected entity count.**
    `slots` vector grows dynamically. Reserve to expected max.

42. **MED — Use a `std::vector<bool>` or bitset for "has valid entity" instead of checking `s.ent` pointer.**
    Checking `shared_ptr` truthiness loads the control block. A separate bitset is cache-friendlier.

43. **LOW — Align `Slot` struct to cache line boundaries for hot-path access.**

44. **LOW — Consider making `EntityHandle` resolution branchless.**
    The `is_invalid()` + bounds check + generation check is 3 branches. Pack slot+gen into a single 64-bit compare.

### Snapshot

45. **MED — Avoid calling `EntityHelper::merge_entity_arrays()` inside snapshot if already merged.**
    Track a "dirty" flag that's set when `temp_entities` is non-empty.

46. **LOW — Pre-allocate `out` vector in `take_entities` / `take_components` to entity count.**
    Already done for `take_entities` but not for `take_components`.

### Text Cache

47. **MED — Use open-addressing hash map (e.g., `robin_map`) instead of `std::unordered_map` for text cache.**
    `std::unordered_map` is node-based with poor cache locality. Robin hood or Swiss table is 2-3x faster for lookups.

48. **MED — Avoid FNV-1a byte-at-a-time hashing; use a SIMD-friendly hash (xxHash, wyhash).**
    The current `compute_hash` iterates byte-by-byte over text + font name. A bulk hash function is significantly faster for longer strings.

49. **LOW — Batch prune operations with amortized cleanup instead of per-entry iteration.**
    `prune_stale_entries` iterates every entry. Use a timestamp-sorted secondary structure or LRU list for O(1) eviction.

50. **LOW — Use `string_view` keys to avoid string copies in hot paths.**
    Already uses `string_view` parameters but the hash function could benefit from avoiding copies to `std::string` in the map key.

---

## Collision Plugin

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

---

## Pathfinding Plugin

1. **HIGH — Replace `std::set<Vec2>` with a flat hash set or grid-based visited array.**
   `std::set` does red-black tree operations with O(log N) per insert/lookup. A hash set or direct 2D array is O(1).

2. **HIGH — Replace `std::map<Vec2, ScoreValue>` in A\* with flat arrays or hash maps.**
   `g_score` and `f_score` use `std::map` (red-black tree). For grid-based pathfinding, a 2D array is O(1) and cache-friendly.

3. **HIGH — Replace `std::set` open_set in A\* with a priority queue (min-heap).**
   `get_lowest_f` does a linear scan of the open set every iteration. A binary heap gives O(log N) extraction.

4. **HIGH — Use `new Node` allocations in BFS with a pool allocator or arena.**
   BFS allocates `new Node` for every visited cell and deletes them all at the end. An arena allocator eliminates per-node malloc/free overhead.

5. **HIGH — Avoid `std::function` callbacks for `is_walkable`, `distance_fn`, `get_neighbors_fn`.**
   These are called per-node in the inner loop. Template parameters or function pointers eliminate virtual dispatch.

6. **MED — Pre-allocate neighbor vectors in `get_neighbors_fn` instead of creating a new vector each call.**
   The default neighbor function creates a `std::vector<Vec2>` with 8 elements on every call. Use a fixed-size array.

7. **MED — Use Manhattan distance for grid-based heuristics instead of Euclidean.**
   `std::sqrt(dx*dx + dy*dy)` is expensive. Manhattan distance is faster and still admissible for grid movement.

8. **MED — Implement Jump Point Search (JPS) for uniform-cost grids.**
   JPS can be 10-100x faster than A* on uniform grids by skipping symmetric paths.

9. **MED — Use bidirectional search for long paths.**
   Search from both start and end simultaneously; meet in the middle for ~50% reduction in explored nodes.

10. **MED — Limit BFS search radius more aggressively.**
    The `dist_sq > (max_path_length * max_path_length)` check compares distance, not path length. Use actual BFS depth.

11. **MED — Cache paths for entities that request the same start/end repeatedly.**

12. **MED — Use hierarchical pathfinding (HPA\*) for large maps.**
    Divide the map into regions, pathfind between regions first, then locally.

13. **MED — Replace `std::deque<Vec2>` paths with `std::vector<Vec2>`.**
    `deque` has per-chunk allocation overhead. Paths are typically built once and iterated, making `vector` more cache-friendly.

14. **MED — Use a condition variable instead of busy-wait polling in the pathfinding thread.**
    The thread currently spins with `sleep_for(1ms)` when idle. A condition variable wakes it immediately when work arrives.

15. **MED — Batch multiple path requests per thread wake-up.**
    Process all pending requests in one batch rather than waking per-request.

16. **MED — Replace `AtomicQueue` with a lock-free SPSC queue.**
    Single-producer single-consumer queues avoid mutex overhead entirely.

17. **MED — Use integer coordinates instead of float `Vec2` for grid pathfinding.**
    Integer comparison and hashing is faster than float.

18. **LOW — Pre-compute walkability into a bitset for O(1) lookup.**

19. **LOW — Use A\* with a Fibonacci heap for better theoretical complexity.**

20. **LOW — Smooth paths after computation (funnel algorithm) to reduce waypoint count.**

21. **LOW — Avoid `std::deque<Node*>` for BFS queue; use a `std::vector` with front pointer.**

22. **LOW — Reuse pathfinding data structures across requests (avoid reallocating maps/sets).**

23. **LOW — Use `reserve()` on the path result before reconstruction.**

24. **LOW — Inline the `reconstruct_path` lambda.**

25. **LOW — Use thread-local storage for pathfinding scratch buffers.**

26. **LOW — Profile and tune `MAX_ITERATIONS` (10000) — may be too high for simple maps.**

27. **LOW — Cache `is_walkable` results for recently queried cells.**

28. **LOW — Use `int16_t` for grid coordinates to halve memory usage in visited sets.**

29. **LOW — Avoid copying `std::function` in `PathRequest`; use `std::move`.**

30. **LOW — Use SIMD for distance calculations when processing multiple neighbors.**

31. **LOW — Pre-sort neighbors by heuristic distance to goal for better A\* node ordering.**

32. **LOW — Implement path request coalescing: if multiple entities want the same path, compute once.**

33. **LOW — Use memory-mapped walkability data for large maps.**

34. **LOW — Profile lock contention on `AtomicQueue` and consider reducing lock granularity.**

35. **LOW — Avoid `std::chrono::high_resolution_clock::now()` calls on every loop iteration; use a coarser timer.**

---

## Input Plugin

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

---

## Animation Plugin

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

---

## UI / AutoLayout Plugin

1. **HIGH — Avoid per-frame `EntityQuery` scans for UI entities; maintain a persistent UI entity list.**
   `BuildUIEntityMapping` runs a full `EntityQuery` every frame to rebuild the `UIEntityMappingCache`. Instead, maintain the list incrementally (add on create, remove on destroy).

2. **HIGH — Replace `std::map<EntityID, RefEntity>` in `UIEntityMappingCache` with a flat hash map or direct array.**
   `std::map` is a red-black tree. Entity IDs are integers; a hash map or sparse array is O(1).

3. **HIGH — Cache autolayout results and only re-run when the UI tree changes (dirty flag).**
   `AutoLayout::autolayout()` runs every frame for every root. Add a dirty flag system so unchanged subtrees skip layout.

4. **HIGH — Avoid repeated `to_cmp()` / `to_ent()` lookups in recursive layout functions.**
   Every layout function calls `this->to_cmp(child_id)` which does a map lookup. Pass references down the recursion instead.

5. **HIGH — Reduce the number of recursive passes in autolayout.**
   Currently: `reset_computed_values` → `calculate_standalone` → `calculate_those_with_parents` → `calculate_those_with_children` → `solve_violations` → `compute_relative_positions` → `compute_rect_bounds`. That's 7 passes over the tree. Combine passes where possible.

6. **MED — Use arena allocation for UI entities instead of `shared_ptr`.**
   UI entities are created/destroyed every frame in immediate mode. Arena allocation eliminates per-entity heap allocation.

7. **MED — Replace `std::map<UI_UUID, EntityID>` in `existing_ui_elements` with a flat hash map.**
   `std::map` has O(log N) lookups. This is hit for every UI element every frame.

8. **MED — Avoid `std::stringstream` + `std::hash<string>` for UI element hashing in `mk()`.**
   `mk()` creates a `stringstream`, writes to it, converts to string, then hashes. Pre-compute a numeric hash from the source location directly (combine file hash + line + column + parent id).

9. **MED — Cache `EntityHelper::get_singleton_cmp<UIContext>()` lookups per frame.**
   Multiple systems call this every frame. Cache the pointer in `once()`.

10. **MED — Skip hidden subtrees entirely in layout passes.**
    `should_hide` children still get recursed into during layout. Skip the entire subtree.

11. **MED — Use `std::vector<UIComponent*>` for children instead of `std::vector<EntityID>`.**
    Avoids the `to_cmp(child_id)` map lookup for every child access during layout.

12. **MED — Pool-allocate `RenderInfo` commands or use a ring buffer.**
    `context.render_cmds` grows/shrinks every frame.

13. **MED — Sort render commands once using radix sort instead of `std::sort`.**
    Layer values are small integers; radix sort is O(N).

14. **MED — Avoid `find_clip_ancestor` / `find_scroll_view_ancestor` walking up the tree per-entity.**
    Cache the clip ancestor on the UIComponent during layout.

15. **MED — Avoid `compute_effective_opacity` walking up the tree per-entity during rendering.**
    Compute and cache cumulative opacity during layout.

16. **MED — Use `SmallVector<EntityID, 8>` for UIComponent::children.**
    Most UI elements have < 8 children. Avoids heap allocation.

17. **MED — Batch text measurement calls; avoid redundant measure_text for the same string.**
    The `TextMeasureCache` helps but `position_text_ex` still calls `measure_text` multiple times (binary search).

18. **LOW — Avoid `fmt::format` in layout warning messages unless actually logging.**
    `get_child_debug_name` / `get_parent_debug_name` create `fmt::format` strings even when the warning isn't triggered.

19. **LOW — Use `constexpr` for layout constants (GRID_UNIT_720P, BASE_WRAP_TOLERANCE, etc.).**

20. **LOW — Replace `std::bitset<4>` for corner settings with a plain `uint8_t` and bit ops.**

21. **LOW — Avoid `std::optional<TextStroke>` / `std::optional<TextShadow>` in rendering hot path; use null sentinels.**

22. **LOW — Pre-compute `fetch_screen_value_` once per layout call instead of per-widget.**

23. **LOW — Avoid `std::array<float, 4>` temporaries in `for_each_spacing`; write directly.**

24. **LOW — Use `memset` to reset computed values instead of per-field assignment.**

25. **LOW — Avoid `std::source_location` overhead in release builds for `mk()`.**

26. **LOW — Profile the `solve_violations` loop (up to 10 iterations) and tune convergence.**

27. **LOW — Use `[[likely]]` for the common case of `!child.absolute && !child.should_hide`.**

28. **LOW — Cache `font_manager.get_active_font()` per frame instead of per-text-draw.**

29. **LOW — Avoid `push_rotation` / `pop_rotation` calls when rotation is 0.**

30. **LOW — Use hardware scissor test batching to reduce state changes.**

31. **LOW — Profile `draw_rectangle_rounded` segment count and reduce for small elements.**

32. **LOW — Avoid checking `entity.has<X>()` repeatedly in `render_me`; combine into a single bitmask check.**

33. **LOW — Use `string_view` for `HasLabel::label` to avoid string copies.**

34. **LOW — Avoid `RenderCommandBuffer` re-creation every frame; reuse and clear.**

35. **LOW — Move the bubble-sort fallback (`#if __WIN32`) to use `std::sort` on modern MSVC.**

---

## Sound System Plugin

1. **HIGH — Avoid linear scan in `play_if_none_playing` and `play_first_available_match`.**
   Both iterate all matches checking `IsSoundPlaying`. Cache which sounds are playing.

2. **MED — Replace `std::map<string, vector<string>>` in `SoundEmitter` with flat hash maps.**
   `std::map` is a tree with O(log N) lookups and string comparison overhead.

3. **MED — Pre-compute alias names at load time, not on first play.**
   `ensure_alias_names` runs lazily on first play, causing a stutter.

4. **MED — Avoid `std::string` copies in `play_with_alias_or_name`.**
   `const std::string base{name}` copies the C-string.

5. **MED — Cache `SoundLibrary::get()` singleton access in local variable.**
   Multiple calls to `SoundLibrary::get()` in the same function.

6. **MED — Use `string_view` for sound name lookups to avoid allocation.**

7. **MED — Avoid `try/catch` blocks in `play_with_alias_or_name`; use `contains()` check.**
   Exception handling has significant overhead even when not thrown.

8. **MED — Batch `MusicLibrary::update()` to only update actively playing streams.**

9. **LOW — Replace `std::multimap` in `Library` base with a flat hash multimap.**

10. **LOW — Pool-allocate `PlaySoundRequest` components.**

11. **LOW — Avoid `removeComponent<PlaySoundRequest>` after every play; use a "processed" flag.**

12. **LOW — Use `std::string_view` in `Library::load` / `Library::get` to avoid copies.**

13. **LOW — Pre-size `alias_names_by_base` reserve based on expected sound count.**

14. **LOW — Avoid modular arithmetic in round-robin alias selection; use branch-free increment.**

15. **LOW — Cache volume settings to avoid redundant `SetSoundVolume` calls.**

16. **LOW — Use a lock-free queue for sound playback requests from other threads.**

17. **LOW — Avoid `std::to_string` in alias name generation; use `fmt::format_int`.**

18. **LOW — Skip `MusicUpdateSystem` when no music is loaded.**

19. **LOW — Profile `raylib::LoadSound` and consider async loading.**

20. **LOW — Deduplicate sound loads (multiple loads of the same file).**

21. **LOW — Use memory-mapped files for large audio assets.**

22. **LOW — Pre-load commonly used sounds at startup instead of on-demand.**

23. **LOW — Reduce singleton lookup overhead with a cached pointer.**

24. **LOW — Avoid `std::string` concatenation in `ensure_alias_names`; use fixed buffer.**

25. **LOW — Use `constexpr` for default volume values.**

26. **LOW — Profile and tune `default_alias_copies` (4) per game requirements.**

27. **LOW — Add a sound priority system to skip low-priority sounds when many are playing.**

28. **LOW — Use spatial audio culling to skip sounds outside the listener's range.**

29. **LOW — Batch volume updates instead of iterating all sounds per change.**

30. **LOW — Avoid `auto &kv : storage` in volume update; use index-based iteration.**

31. **LOW — Cache the `SoundEmitter` singleton pointer in `SoundPlaybackSystem::once()`.**

32. **LOW — Use `std::move` for `PlaySoundRequest::name` / `prefix` fields.**

33. **LOW — Replace `Policy` enum dispatch with a vtable-free approach (template parameter).**

34. **LOW — Pre-sort sounds by name for faster prefix matching.**

35. **LOW — Consider using audio middleware (FMOD/Wwise) for better performance at scale.**

---

## Texture Manager Plugin

1. **MED — Batch sprite draw calls with the same texture.**
   `RenderSprites` and `RenderAnimation` draw one sprite at a time. Batch all sprites sharing the same spritesheet into a single draw call.

2. **MED — Cache the spritesheet texture in the system instead of querying singleton every frame.**
   `once()` queries the singleton every frame. Cache it once at system creation.

3. **MED — Use texture atlases to reduce draw call count.**

4. **MED — Pre-compute `destination()` rectangle instead of computing per-frame for static sprites.**

5. **MED — Use instanced rendering for sprites with the same frame.**

6. **MED — Sort sprites by texture before rendering to minimize state changes.**

7. **MED — Pre-compute `idx_to_sprite_frame` results into a lookup table.**
   Already `constexpr`, but ensure it's actually evaluated at compile time.

8. **LOW — Use `float` directly instead of `(float)int` casts in frame calculation.**

9. **LOW — Avoid `std::pair<int,int>` return in `idx_to_next_sprite_location`; use a struct.**

10. **LOW — Skip rendering off-screen sprites (frustum culling).**

11. **LOW — Use GPU instancing for large numbers of identical sprites.**

12. **LOW — Pool-allocate `HasSprite` / `HasAnimation` components.**

13. **LOW — Avoid calling `center()` in `destination()`; cache the center position.**

14. **LOW — Use `constexpr` for AFTERHOURS_SPRITE_SIZE_PX calculations.**

15. **LOW — Pre-compute animation frame sequences at load time.**

16. **LOW — Reduce `HasAnimation::Params` struct size by packing fields.**

17. **LOW — Use `uint16_t` for frame indices to reduce memory.**

18. **LOW — Avoid `Vector2Type` copies in `TransformData::center()`; return by value is fine but ensure NRVO.**

19. **LOW — Profile and tune sprite batch sizes for the target GPU.**

20. **LOW — Use a sprite atlas packer to minimize texture waste.**

21. **LOW — Avoid redundant `entity.cleanup = true` for finished one-shot animations; batch cleanup.**

22. **LOW — Pre-compute `frame.width * scale` and `frame.height * scale` once per animation.**

23. **LOW — Use mipmapping for sprites that are frequently scaled down.**

24. **LOW — Avoid `draw_texture_pro` call overhead; batch into a vertex buffer.**

25. **LOW — Profile texture sampling and consider nearest-neighbor for pixel art.**

26. **LOW — Use compressed textures (DXT/ETC) to reduce GPU memory bandwidth.**

27. **LOW — Skip alpha-blending for fully opaque sprites.**

28. **LOW — Pre-load textures in a background thread.**

29. **LOW — Use texture streaming for large open-world maps.**

30. **LOW — Profile and optimize the `HasAnimation` update loop (frame timing).**

31. **LOW — Avoid floating-point animation frame position; use integer frame index.**

32. **LOW — Cache `HasSpritesheet` texture reference to avoid singleton lookup per frame.**

33. **LOW — Use `alignas` on sprite data for better cache line utilization.**

34. **LOW — Consider sprite batching libraries (e.g., raylib's built-in batch mode).**

35. **LOW — Reduce draw call state changes by sorting: texture → blend mode → shader.**

---

## Timer Plugin

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

---

## Camera Plugin

1. **MED — Cache the `HasCamera` singleton pointer instead of querying every frame.**
   `BeginCameraMode::once()` and `EndCameraMode::once()` both call `get_singleton_cmp<HasCamera>()` every frame.

2. **MED — Avoid `BeginMode2D` / `EndMode2D` when the camera hasn't changed.**
   If position, zoom, and rotation are unchanged, skip the state change.

3. **MED — Combine `BeginCameraMode` and `EndCameraMode` into a single RAII system.**
   Reduces the number of systems in the render pipeline.

4. **LOW — Use integer coordinates for pixel-perfect rendering to avoid sub-pixel jitter.**

5. **LOW — Pre-compute the camera matrix once per frame instead of letting raylib recalculate.**

6. **LOW — Cache the view-projection matrix for frustum culling in other systems.**

7. **LOW — Avoid redundant `if (camera_entity)` null checks when singleton is guaranteed.**

8. **LOW — Use `constexpr` for default camera values.**

9. **LOW — Profile `BeginMode2D` overhead and consider manual matrix setup.**

10. **LOW — Implement camera interpolation (lerp) to reduce the number of position updates.**

11. **LOW — Use dirty flag for camera to skip unnecessary `set_position` calls.**

12. **LOW — Pre-compute screen-to-world and world-to-screen transforms for click handling.**

13. **LOW — Avoid floating-point rounding in camera offset calculations.**

14. **LOW — Cache camera bounds (visible rect) for culling optimizations.**

15. **LOW — Use SIMD for camera matrix multiplication.**

16. **LOW — Combine camera transform with sprite rendering to reduce per-sprite matrix operations.**

17. **LOW — Profile and optimize camera shake effects (if used) to avoid per-frame trig calls.**

18. **LOW — Use a camera stack for nested camera contexts (e.g., minimap) to avoid redundant setup.**

19. **LOW — Avoid `EnforceSingleton<HasCamera>` system running every frame; enforce once at init.**

20. **LOW — Pre-compute zoom-dependent constants (tile visibility, LOD levels).**

21. **LOW — Use integer zoom levels for pixel art to avoid texture filtering artifacts.**

22. **LOW — Cache the camera entity reference to avoid singleton map lookup.**

23. **LOW — Profile raylib's `BeginMode2D` implementation for optimization opportunities.**

24. **LOW — Avoid redundant `set_zoom` / `set_rotation` calls when values haven't changed.**

25. **LOW — Use platform-specific optimizations (Metal, Vulkan) for camera matrix setup.**

26. **LOW — Batch camera-dependent calculations (world-to-screen) for all entities at once.**

27. **LOW — Consider using a projection matrix cache that invalidates only on resolution change.**

28. **LOW — Avoid virtual dispatch for camera systems; they're always concrete types.**

29. **LOW — Profile the overhead of two separate systems (begin/end) vs. a single wrapper.**

30. **LOW — Use `alignas` on camera data structures for better cache performance.**

31. **LOW — Consider removing the non-raylib stub path entirely if not used in production.**

32. **LOW — Pre-compute `camera.offset` from window dimensions once per resolution change.**

33. **LOW — Use a ring buffer for camera position history (for smoothing/replay).**

34. **LOW — Inline camera accessor methods (`set_position`, `set_zoom`, etc.).**

35. **LOW — Profile memory access patterns when camera data is accessed alongside transforms.**

---

## Summary: Top 10 Highest-Impact Changes

| # | Area | Optimization | Est. Impact |
|---|------|-------------|-------------|
| 1 | Core | Replace `shared_ptr<Entity>` with pool/arena allocation | **Massive** — eliminates refcount traffic + heap fragmentation |
| 2 | Core | SoA component storage (archetype-based) | **Massive** — cache-friendly iteration for all systems |
| 3 | Core | Component index lists (skip non-matching entities) | **Huge** — systems only see entities they care about |
| 4 | Core | Batch `merge_entity_arrays()` (once per phase, not per system) | **High** — removes O(N) work per system |
| 5 | Core | Bitset-based system matching (remove per-entity `has<T>()` calls) | **High** — single instruction replaces N checks |
| 6 | Collision | Spatial hash grid for broad-phase | **Huge** — O(N) vs O(N^2) |
| 7 | Collision | Cache collidable entity list per-frame | **High** — removes N full-entity-scan queries |
| 8 | Pathfinding | Priority queue + hash set for A* | **High** — O(N log N) vs O(N^2) |
| 9 | UI | Incremental layout (dirty flags) | **High** — skip unchanged subtrees |
| 10 | UI | Replace `std::map` in entity mapping with flat hash map | **High** — O(1) vs O(log N) per child lookup |
