# Core ECS

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
