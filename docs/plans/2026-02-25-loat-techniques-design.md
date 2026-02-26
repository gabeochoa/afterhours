# Large Arrays of Things — Techniques Worth Stealing

**Date:** 2026-02-25
**Source:** "Baby's First ECS" / "Large Arrays of Things" video series
**Scope:** Analysis of fat-struct + slot-map + intrusive-list patterns and what afterhours can leverage

---

## Context

The video presents a minimalist alternative to traditional ECS: a flat array of "things" (fat structs) with slot-map allocation, generation-counter references, intrusive linked lists for hierarchy, and nil sentinels. The philosophy is: keep it simple, keep it contiguous, avoid pointer indirection.

Afterhours is a composable ECS framework, so adopting the full fat-struct model doesn't fit. But several specific techniques address real pain points in the current codebase. This doc covers what's worth taking, what isn't, and why.

---

## 1. Intrusive Linked Lists for UI Hierarchy

### Priority: HIGH
### Effort: LARGE (70+ call sites to migrate)

### Current State

`UIComponent` stores parent-child relationships as:

```cpp
EntityID parent = -1;
std::vector<EntityID> children;
```

The `children` vector is:
- **Cleared every frame** by `ClearUIComponentChildren`
- **Rebuilt every frame** via `add_child()` during component init
- **Iterated 70+ times** across autolayout (12 sites), rendering (9), systems (28), validation, drag-and-drop, layout inspector, etc.

Each `std::vector<EntityID>` means a heap allocation per UI entity. For a UI with 200 elements, that's 200 heap allocs per frame just for the children vectors (clear + rebuild pattern).

### Proposed Change

Replace `std::vector<EntityID> children` with intrusive linked list indices stored directly in `UIComponent`:

```cpp
EntityID parent = -1;
EntityID first_child = -1;
EntityID next_sibling = -1;
EntityID prev_sibling = -1;
```

Operations become:
- **add_child(child_id):** Prepend to `first_child` list. O(1).
- **remove_child(child_id):** Unlink via `prev_sibling`/`next_sibling`. O(1).
- **iterate children:** Walk `first_child` → `next_sibling` → `next_sibling` → ...
- **clear children:** Walk list and reset links, or just null `first_child` and reset during next rebuild.

### Benefits

- **Zero heap allocation** for hierarchy storage
- **O(1) insert and remove** (no vector resize, no erase-from-middle)
- **Eliminates per-frame clear+rebuild cost** of vector capacity management
- **Single ownership enforcement** — a node can only be in one list at a time, preventing accidental double-parenting bugs

### Challenges

| Challenge | Mitigation |
|-----------|------------|
| `children.size()` used in autolayout for size calculations | Add a `child_count` field maintained during add/remove, or walk the list (typically < 20 children) |
| `children.empty()` checks | `first_child == -1` |
| `children[i]` indexed access (drag spacer insertion) | Walk list to position — only used in drag-and-drop, not hot path |
| `children.insert(pos, id)` in drag spacer | Walk to position, splice. Rare operation. |
| Ordered iteration (autolayout needs stable order) | Intrusive list preserves insertion order naturally. Append-to-tail via a `last_child` pointer if needed. |
| 70+ call sites to migrate | Wrap in iterator: `for (EntityID cid : children_of(widget))` abstracts the storage |

### Migration Strategy

1. Add `first_child`/`next_sibling`/`prev_sibling` fields to `UIComponent`
2. Create `children_of(UIComponent&)` iterator that yields EntityIDs by walking the list
3. Rewrite `add_child()` and `remove_child()` to maintain intrusive links
4. Migrate call sites from `for (EntityID child_id : widget.children)` to `for (EntityID child_id : children_of(widget))`
5. Once all sites are migrated, remove `std::vector<EntityID> children`
6. Optionally add `last_child` for O(1) append (preserves insertion order for autolayout)

### Circular vs Non-Circular

The video discusses circular doubly linked lists to eliminate null checks during traversal. For the UI hierarchy this is probably overkill — the sentinel `-1` check is fine and non-circular lists are easier to reason about. Keep it simple.

---

## 2. Nil Sentinel Entity

### Priority: MEDIUM
### Effort: MEDIUM (audit all -1 sentinel comparisons)

### Current State

Afterhours uses multiple sentinel values:
- `EntityID parent = -1` (no parent)
- `EntityHandle::INVALID_SLOT = max uint32` (no slot)
- `OptEntity` wrapper with `std::optional` semantics
- Explicit `if (pid >= 0)` guards scattered through UI code

### Proposed Change

Reserve EntityID 0 as a nil sentinel. Start `ENTITY_ID_GEN` at 1 instead of 0. Zero-initialize all EntityID fields.

```cpp
// Before
EntityID parent = -1;
if (parent >= 0) { ... }

// After
EntityID parent = 0;  // zero-init = nil
if (parent) { ... }   // natural truthy check
```

Optionally create a static nil entity at slot 0 that is always valid but represents "nothing" — accessing it returns safe defaults instead of crashing.

### Benefits

- **Zero-initialization is the safe default** — no need to remember to set `-1`
- **Truthy checks** instead of comparison to magic value
- **Debug protection:** mark nil entity memory as read-only to catch accidental writes
- **Aligns with intrusive list nil** — zero means "end of list" naturally

### Challenges

- Current EntityID 0 is a real entity (first one created). Would need to start at 1.
- Every `== -1` and `>= 0` check in the codebase needs auditing.
- `OptEntity` already provides safe null handling — nil sentinel is a different philosophy.

### Recommendation

Do this alongside the intrusive list migration (item 1). If intrusive lists use 0 as nil, the hierarchy code becomes cleaner. Don't retrofit to the entire ECS — just the UI hierarchy and any new systems.

---

## 3. Pool/Arena Allocation for Entities

### Priority: HIGH (for perf)
### Effort: VERY LARGE (foundational change)

### Current State

Every entity is created via `std::make_shared<Entity>()`. Each entity is ~1.1 KiB (128 `unique_ptr<BaseComponent>` + bitsets + tags + ID). The `shared_ptr` adds atomic ref-count overhead on every copy/move.

Entity storage: `std::vector<std::shared_ptr<Entity>>` — two levels of pointer indirection to reach entity data.

### What the Video Does

Fixed-size flat array of entities. No heap allocation per entity. Slot map with generation counters for safe references. Free list through unused slots.

### What Afterhours Could Do

Replace `vector<shared_ptr<Entity>>` with a pool allocator:

```
Option A: std::vector<Entity> + stable handles (simplest)
Option B: Fixed-size arena with slot map (video's approach)
Option C: Custom allocator with allocate_shared (smallest change)
```

This is already identified as items 1, 9, 10 in `docs/speed/core_ecs.md`. The video provides a concrete implementation pattern to follow.

### Dependency

This is best done alongside the SoA migration (speed doc item 2) since the Entity struct is so large. Doing pool allocation on 1.1 KiB entities helps but doesn't fix the fundamental cache locality problem of AoS.

### Recommendation

Don't do this in isolation. Plan it as part of the larger ECS storage rework (SoA + pool alloc + remove BaseComponent inheritance). The video's slot-map pattern is the right target architecture for that work.

---

## 4. Intrusive Free List for Slot Allocation

### Priority: LOW
### Effort: SMALL

### Current State

`EntityCollection` already has a working free slot system:

```cpp
std::vector<Slot> slots;
std::vector<EntityHandle::Slot> free_slots;  // separate vector
```

Allocation is O(1) (pop from `free_slots`). Deallocation is O(1) (push to `free_slots`).

### What the Video Does

Stores the next-free-slot index *inside* the unused slot itself (intrusive). No separate free list vector needed.

```cpp
union {
    EntityType ent;          // when slot is occupied
    Slot next_free;          // when slot is empty
};
```

### Assessment

The current implementation already works. The separate `free_slots` vector costs a few KB of memory at most. Making the free list intrusive saves that vector but adds implementation complexity (union or reinterpret).

**Not worth doing now.** File this under "nice to know" for a future rewrite.

---

## 5. Lightweight Thing Pool for Hot-Path Subsystems

### Priority: MEDIUM-HIGH
### Effort: MEDIUM (new code, no migration needed)

### Motivation

The full ECS path for creating an entity is:
1. `make_shared<Entity>()` — heap alloc
2. `addComponent<T>()` — `make_unique<T>()` — another heap alloc per component
3. Store in `temp_entities` vector
4. Merge into `entities_DO_NOT_USE` after system tick
5. Iterate all entities for every system

For particles, projectiles, damage numbers, trail points — objects that are all the same shape, created/destroyed in bulk, and don't need runtime component composition — this is massive overkill.

### Proposed Design

A standalone `ThingPool<T, MaxCount>` template that lives alongside the ECS:

```cpp
template<typename T, size_t MaxCount = 4096>
struct ThingPool {
    struct ThingRef {
        uint32_t index;
        uint32_t generation;
        operator bool() const { return index != 0; }
    };

    // Flat array. Slot 0 is nil sentinel.
    T things[MaxCount];
    uint32_t generations[MaxCount];
    bool alive[MaxCount];

    // Intrusive free list
    uint32_t free_head = 1;
    uint32_t next_free[MaxCount];
    uint32_t count = 0;

    ThingRef add(T&& thing);
    void remove(ThingRef ref);
    T* resolve(ThingRef ref);

    template<typename Fn>
    void for_each(Fn&& fn);  // skips dead slots
};
```

### Usage Example

```cpp
struct Particle {
    vec2 pos, vel;
    float lifetime, alpha;
    Color color;
};

// In game state
ThingPool<Particle, 8192> particles;

// Spawn
auto ref = particles.add({
    .pos = emitter_pos,
    .vel = random_dir() * speed,
    .lifetime = 1.0f,
    .alpha = 1.0f,
    .color = colors::white,
});

// Update system (not an ECS system — just a function)
particles.for_each([dt](Particle& p) {
    p.pos += p.vel * dt;
    p.lifetime -= dt;
    p.alpha = p.lifetime;
    if (p.lifetime <= 0) p.alive = false; // or use a return value
});

// Render
particles.for_each([](const Particle& p) {
    draw_particle(p.pos, p.color, p.alpha);
});
```

### Benefits

- **Zero heap allocation** — fixed array
- **Dense iteration** — no pointer chasing, no component lookup
- **Generation-safe references** — same safety as ECS handles
- **Simple API** — no systems to register, no components to define
- **10-100x faster** than full ECS for uniform high-count objects

### When to Use Thing Pool vs ECS

| Use ECS when... | Use ThingPool when... |
|---|---|
| Objects have varying component sets | All instances have identical data layout |
| Need to query by component | Just need to iterate all |
| Moderate count (< 1000) | High count (1000+) |
| Complex lifetime (permanent, singleton) | Simple lifetime (spawn, tick, die) |
| Need system scheduling | Just need a for_each loop |

### Integration with ECS

A ThingPool can be stored as an ECS singleton component, letting ECS systems read/write it:

```cpp
struct ParticlePool : BaseComponent {
    ThingPool<Particle, 8192> pool;
};

struct UpdateParticles : System<ParticlePool> {
    void for_each_with(Entity&, ParticlePool& pp, float dt) {
        pp.pool.for_each([dt](Particle& p) { ... });
    }
};
```

This gives you ECS scheduling and system ordering while keeping the actual particle data in a dense pool.

---

## 6. Summary: What to Do and When

### Phase 1 — Quick Wins (can do now, independently)

| Item | Value | Effort | Notes |
|------|-------|--------|-------|
| ThingPool template | HIGH | MEDIUM | New code, no migration. Start with particles. |
| Nil sentinel for new code | LOW | SMALL | Adopt in new systems, don't retrofit. |

### Phase 2 — UI Hierarchy Rework

| Item | Value | Effort | Notes |
|------|-------|--------|-------|
| Intrusive linked list for UIComponent | HIGH | LARGE | 70+ call sites. Use iterator abstraction for migration. |
| Nil sentinel for UI EntityIDs | MEDIUM | MEDIUM | Do alongside intrusive lists. |

### Phase 3 — Core ECS Storage Overhaul

| Item | Value | Effort | Notes |
|------|-------|--------|-------|
| Pool/arena allocation | HIGH | VERY LARGE | Part of SoA migration. See `docs/speed/core_ecs.md`. |
| Remove BaseComponent inheritance | HIGH | VERY LARGE | Same migration. |
| Intrusive free list | LOW | SMALL | Nice cleanup during rewrite. |

---

## 7. Ideas NOT Worth Adopting

### Full Fat Struct (One Struct to Rule Them All)

Afterhours is a reusable framework used across multiple projects (kart, tetr, wm, ui, pong). Component composition is the core value proposition. A fat struct forces all projects to share one monolithic entity definition. Hard pass.

### Static Fixed-Size Entity Array

The video advocates `Thing things[MAX_THINGS]` with no dynamic allocation. For a library that's used in projects ranging from 50 to 50,000 entities, a compile-time fixed max is too rigid. Dynamic growth with pool allocation is the right middle ground.

### Abandoning std::shared_ptr Entirely

`shared_ptr` provides automatic lifetime management that simplifies the current ownership model. Removing it requires a complete rethink of entity ownership (who decides when an entity is truly dead?). Worth doing eventually (Phase 3) but not as a standalone change.

---

## 8. Key Takeaways from the Video

1. **Indices beat pointers.** Afterhours already uses EntityID indices for the UI hierarchy. The intrusive list extension of this principle eliminates the remaining pointer-heavy data structure (the children vector).

2. **Generation counters are non-negotiable.** Already implemented in EntityHandle. Good.

3. **Zero-init + nil sentinel makes code cleaner.** Not a performance win — a correctness and readability win. Worth adopting for new code.

4. **Not everything needs to be an entity.** High-volume uniform objects (particles, trails, damage numbers) are better served by a simple pool than by the full ECS machinery. ThingPool gives you the performance of the video's approach for the cases where it matters, without sacrificing ECS composability for everything else.

5. **Intrusive data structures eliminate allocations.** The children vector in UIComponent is the single highest-impact application of this principle in afterhours today.
