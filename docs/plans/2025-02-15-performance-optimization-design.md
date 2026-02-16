# Performance Optimization Plan

Phased approach to fix low frame rate with 500+ entities. Each phase preserves more API surface than the next.

**Constraint:** No customization loss. Existing plugin/game code must continue to work through Phases 1 and 2. Phase 3 is planned API breakage.

---

## Phase 1: No-API-Change Quick Wins

Internal-only changes. No plugin or game code modifications required.

### 1. Dirty-flag guard on `merge_entity_arrays()`
**Source:** [Core ECS item 6](../speed/core_ecs.md)

Add `bool temp_dirty = false` to the entity collection. Set to `true` when anything is pushed to `temp_entities`. In `merge_entity_arrays()`, early-return if `!temp_dirty`. Reset after merge.

- No behavior change: systems that spawn entities still get the merge
- Systems that don't spawn entities skip the O(N) work entirely

### 2. Skip systems with zero matching entities
**Source:** [Core ECS item 31](../speed/core_ecs.md)

Maintain `std::array<int, AFTER_HOURS_MAX_COMPONENTS>` population counts. Increment on `addComponent<T>()`, decrement on `removeComponent<T>()` and entity cleanup. Before a system iterates, check `population[component_id] > 0` for all required components. If any is zero, skip the system entirely.

### 3. `permanant_ids`: `std::set` -> `std::unordered_set`
**Source:** [Core ECS item 12](../speed/core_ecs.md)

One-line type change. O(1) lookups instead of O(log N).

### 4. Collision `ids`: `std::set` -> `std::unordered_set`
**Source:** [Collision item 4](../speed/collision.md)

Same pattern. O(1) instead of O(log N) per `contains()` in the hot collision loop.

### 5. Cache collidable entity list in `once()`
**Source:** [Collision item 3](../speed/collision.md)

Move the `EntityQuery().whereHasComponent<Transform>()` call from per-entity into the system's `once()` method. Store the result. Iterate that cached list in the inner loop instead of running N full-entity-scan queries per frame.

### 6. UI `UIEntityMappingCache`: `std::map` -> `std::unordered_map`
**Source:** [UI/AutoLayout item 2](../speed/ui_autolayout.md)

Type swap. O(1) lookups instead of O(log N). UI layout hits this map heavily per frame.

### 7. Flatten `singletonMap` to `std::array<Entity*, MAX>`
**Source:** [Core ECS item 15](../speed/core_ecs.md)

Replace `std::unordered_map<ComponentID, Entity*>` with `std::array<Entity*, AFTER_HOURS_MAX_COMPONENTS>`. ComponentID is a small integer, array gives O(1) with zero hash overhead. Initialize to `nullptr`.

---

## Phase 2: Deeper Internal Optimizations (No API Breakage)

Larger internal changes. Public API stays the same or is only extended (additive).

### 8. Template-based query filters
**Source:** [Core ECS item 22](../speed/core_ecs.md)

Replace internal `std::function<bool(const Entity&)>` filter storage with a template-based filter chain that inlines. The public `whereLambda()` API stays the same. The change is how filters are stored and invoked internally, eliminating virtual dispatch and heap allocation per filter.

### 9. `gen_for_each(callback)` to avoid materializing vector
**Source:** [Core ECS item 27](../speed/core_ecs.md)

Add a new method alongside `gen()` that takes a callback instead of returning a `RefEntities` vector. Existing `gen()` stays unchanged. Callers can opt in to the faster path. Additive API.

### 10. Cache `inv_mass` per entity per frame
**Source:** [Collision item 7](../speed/collision.md)

Compute `1.0f / mass` once per entity in the collision system's `once()` method. Store in a side array or temporary cache. The per-pair `resolve_collision` reads the cached value instead of recomputing.

### 11. Early-out for infinite+infinite mass pairs
**Source:** [Collision item 8](../speed/collision.md)

In `resolve_collision`, if both entities have `mass == numeric_limits<float>::max()`, skip the entire impulse/friction calculation. Simple `if` guard at the top.

### 12. Singleton lookup for input collector
**Source:** [Input item 1](../speed/input.md)

Replace the full `EntityQuery` entity scan in `get_input_collector()` with `EntityHelper::get_singleton_cmp<InputCollector>()`. The code already has a TODO comment for this. One-line fix.

### 13. Cache mouse position per frame
**Source:** [Input item 2](../speed/input.md)

Store the computed mouse position in a frame-local variable. Return the cached value on subsequent calls within the same frame. The scaling math (aspect ratio, letterboxing) only needs to run once per frame.

### 14. Dirty flag for animation manager
**Source:** [Animation item 20](../speed/animation.md)

Track `bool has_active_tracks`. Set on `play()`/`from()`/`to()`, clear when all tracks finish. Skip the entire hash map iteration in `update()` when no tracks are active.

---

## Phase 3: Architectural Changes (API-Breaking, Planned)

These are the highest-impact changes from the Top 10 table. They change the public API surface and should be planned and executed individually.

### 15. Replace `shared_ptr<Entity>` with arena/pool allocation
**Source:** [Core ECS item 1](../speed/core_ecs.md)

Eliminate refcount traffic and heap fragmentation. Entities live in a contiguous pool. The existing handle system (`EntityHandle` with slot + generation) becomes the primary reference mechanism. This touches every system that stores entity references.

**Impact:** Massive -- eliminates refcount traffic + heap fragmentation for all 500+ entities.

### 16. SoA component storage / archetype tables (NEEDS INVESTIGATION)
**Source:** [Core ECS items 2 + 4](../speed/core_ecs.md)

Move from per-entity `ComponentArray[128]` to per-archetype dense arrays. Entities with the same component signature share a table. Iteration becomes a tight loop over contiguous memory.

**Impact:** Massive -- cache-friendly iteration for all systems.

**Open question:** Users own their component types. The engine currently stores components as `unique_ptr<BaseComponent>`, which lets users define arbitrary component structs. Archetype/SoA storage requires the engine to manage component memory layout. Three candidate approaches to investigate:

- **Option A: Type-erased placement-new** -- At `addComponent<T>()` time, the engine already knows `sizeof(T)` and `alignof(T)` via templates. It can allocate from a per-type contiguous buffer using placement-new. Users define any struct they want; the engine just needs the type at compile time (which it already has). No `BaseComponent` inheritance needed. This is the approach `entt` uses.
- **Option B: Explicit registration** -- Users register component types upfront with a macro/call that captures size+alignment. Engine pre-allocates typed pools. More explicit but requires a registration step users don't currently need.
- **Option C: Hybrid** -- Keep current `unique_ptr<BaseComponent>` for user components, but add an opt-in SoA path for engine-internal components (Transform, etc.) that the engine controls. Two storage paths coexist. Lower risk but less benefit for user components.

### 17. Per-component-type entity index lists (NEEDS INVESTIGATION)
**Source:** [Core ECS item 23](../speed/core_ecs.md)

Maintain a `vector<Entity*>` per component type. Systems iterate only their component's list instead of scanning all entities.

**Impact:** Huge -- systems only see entities they care about.

**Open question:** Same concern as item 16. The index lists themselves don't restrict component types, but maintaining them requires hooks into `addComponent` / `removeComponent` that track per-type populations. This is more feasible than full SoA -- investigate whether the existing `ComponentBitSet` infrastructure can drive index list maintenance without API changes.

### 18. Spatial hash grid for collision broad-phase
**Source:** [Collision item 1](../speed/collision.md)

Replace O(N^2) all-pairs check with a spatial hash grid. Entities register in cells based on position. Only check pairs within the same or adjacent cells.

**Impact:** Huge -- O(N) vs O(N^2) for collision detection.
