# Remove “serializing pointers” from Afterhours (plan + options)

This doc captures what “serializing pointers” means in this repo today, what we learned from `example/`, what `entity-handle-slots` tried (and why it was perceived as slow), and a phased plan to stop needing pointer-linking serialization **without changing the public API too much and without slowing down games**.

## Problem statement (what we want)

- **Goal**: save-games / network snapshots should contain **no pointer values** and should not require pointer-linking contexts (e.g., Bitsery pointer extension contexts).
- **Constraints**:
  - Keep the **public ECS API** mostly stable (`Entity::addComponent/get/has/remove`, `EntityID`, `OptEntity`, `EntityQuery`, `System<...>`).
  - Avoid slowing down games; ideally this work should be a net performance improvement (less allocation, better cache locality).
  - Components are defined by games, so the library cannot rely on “knowing all component types” at runtime in advance.

## What “serializing pointers” means here (today)

### ECS internal storage is pointer-heavy

Current `Entity` component storage is:

- `componentArray`: `std::array<std::unique_ptr<BaseComponent>, max_num_components>`

This creates two persistent pressures:

- **Serialization pressure**: if a project serializes `Entity` or any object graph containing entities/components, it inevitably encounters `unique_ptr<BaseComponent>` and any pointers those components store.
- **Runtime pressure**: per-component heap allocation (`make_unique<T>`) and pointer chasing on every access.

### `example/` shows the “good direction”: IDs rather than pointers

The UI plugin and examples already model relationships using `EntityID`:

- `UIComponent.parent`, `UIComponent.children`, focus/hot IDs in `UIContext`, etc.

This is pointer-free and serializable by construction. We want to push the whole ecosystem toward this pattern:

- **store IDs/handles** for references
- **re-resolve** through ECS when needed

## Related real-world context (PharmaSea)

PharmaSea contains explicit pointer serialization infrastructure and docs about moving away from it (handle-based store, pointer-free serialization options). It’s a strong signal that:

- pointer-linking contexts are a real maintenance burden
- “stable handles” and “slot+generation” are the right *conceptual* solution

## What `entity-handle-slots` provides (and what looked slow)

The `origin/entity-handle-slots` branch introduces:

- `EntityHandle { uint32_t slot; uint32_t gen; }`
- `Entity::ah_slot_index` runtime metadata
- slot table + freelist + generation bumping on delete
- additive query helpers like `EntityQuery::gen_handles()`

### Why it could feel slow in games (observed issues)

Even with handles, the branch still has several hot-path costs:

- **Entities are still `shared_ptr<Entity>`** everywhere.
  - extra indirection and refcount traffic; also harms cache locality.
- **`getEntityForID` is still O(N)** linear scan over `entities`.
  - UI and gameplay often do lots of ID→entity resolution; this can dominate frame time.
- **`EntityQuery` does extra work**:
  - `has_values()` runs a full query each time.
  - `gen_first()` calls `has_values()` and then effectively runs again.
  - `stop_on_first` option exists but `run_query` does not early-exit.
  - `run_query` always builds an intermediate `RefEntities out` before filtering.
- **Handle validation/logging** in `handle_for` is great for debugging but should be compiled out in release builds.

### Takeaway

`entity-handle-slots` is the right *shape* (slot+generation), but it needs a few structural changes to be fast:

- O(1) `EntityID → handle/slot → entity`
- fewer allocations and fewer full scans
- avoid double-running queries
- remove `shared_ptr` from the iteration hot path if possible

## Strategy overview (phased, compatibility-first)

We can decouple the work into two orthogonal tracks:

1) **Entity identity / references**: move projects from pointers to `EntityHandle` (or equivalent) with O(1) resolution.
2) **Component storage**: move from pointer-per-component to pool-based storage (AoS/SoA-friendly) while preserving API shape.

Doing (1) first removes most “pointer serialization” needs immediately for game code that stores entity references.
Doing (2) later removes the ECS internal pointer pressure and improves performance.

## Phase 1: Fast handle system (slot+generation) with O(1) ID resolution

### Public API (additive)

- Add `afterhours::EntityHandle` (POD): `{slot, gen}`.
- Add:
  - `EntityHelper::handle_for(const Entity&) -> EntityHandle`
  - `EntityHelper::resolve(EntityHandle) -> OptEntity`
  - `EntityHelper::resolveEnforced(EntityHandle) -> RefEntity` (optional)

These are already implemented in `entity-handle-slots` (good starting point).

### Make `EntityID → entity` O(1)

Add a mapping owned by `EntityHelper`:

- **Option A**: `std::vector<uint32_t> id_to_slot;`
  - `id_to_slot[entity.id] = entity.ah_slot_index`
  - on deletion: set to `INVALID_SLOT`
  - `getEntityForID` becomes: `slot = id_to_slot[id]` then `resolve({slot, slots[slot].gen})` (or directly use slot’s `ent` if gen matches)

- **Option B**: `std::vector<uint32_t> id_to_dense_index;`
  - best if dense list is canonical and stable; must update on swap-remove.

This single change removes a major perf cliff in UI and gameplay code.

### Remove “slow query patterns”

Make `EntityQuery` not run multiple times per call chain:

- `gen_first()` should not call `has_values()` (which runs the query) and then run again.
- `has_values()` should be implemented as “try find first match” without building full lists.
- Implement `stop_on_first` as a true early-exit.

### Compile out handle validation in release

Keep correctness checks in debug builds, but in release:

- `handle_for` should be minimal branching
- no logging in hot paths

## Phase 2: Stop storing entity pointers in game components (policy + helpers)

This is the core “stop serializing pointers” requirement at the game layer.

### Recommended rule (document + enforce)

- Don’t store `Entity*`, `RefEntity`, or `shared_ptr<Entity>` inside components that are serialized.
- Store `EntityHandle` (or `EntityID` if you accept stale references) instead.

### Helper patterns

- Store `EntityHandle parent;` in a component.
- On use: `if (auto opt = EntityHelper::resolve(parent)) { ... }`

### Migration approach

- Identify pointer-shaped fields in game components (e.g., `Entity*`, `RefEntity`, `shared_ptr<Entity>`).
- Replace them with:
  - `EntityHandle` if the link should remain valid across deletes/respawns (and detect staleness)
  - `EntityID` if lifetime is tightly controlled and staleness is acceptable/validated elsewhere

## Phase 3: Component storage redesign (SoA/pools) while keeping public API stable

This is the big internal change that removes most ECS-pointer pressure and improves runtime performance.

### Target storage model

For each component type `T`, maintain a `ComponentPool<T>`:

- `std::vector<T> dense;`
- `std::vector<EntityID> dense_to_entity;`
- `std::vector<uint32_t> entity_to_dense;` (sparse; sized to max entity id or grown)

Operations:

- **add**: `dense.emplace_back(args...)`, set sparse index, set entity bit.
- **get**: O(1) index lookup.
- **remove**: swap-remove `dense`, update moved entity’s sparse index, clear bit.

### Why this works when games define components

It’s template-instantiated per component type `T` on demand. The library doesn’t need to “know all components” at runtime.

### Keeping public API shape

Keep existing `Entity` methods (same signatures):

- `addComponent<T>(...)`
- `get<T>()`
- `has<T>()`
- `removeComponent<T>()`

Under the hood, these forward to the pool.

### Caveat: component pointer stability

With dense vectors, component addresses can move (vector growth, swap-remove).

That’s usually fine (and preferable) if projects follow the rule:

- do not store `T*` long-term; store entity handle and re-fetch component.

If needed, provide a temporary compatibility macro:

- `AFTER_HOURS_STABLE_COMPONENT_ADDRESSES` → uses a stable-address container (slower; transitional only).

### Interaction with “derived/child-of” queries

Current `has_child_of/get_with_child` uses RTTI (`dynamic_cast`) over `BaseComponent*`.

Pool-based storage doesn’t naturally support that.

Options:

- **Legacy mode only**: keep “derived children” feature only when using pointer-backed components.
- **Explicit registry**: allow projects to register derived relationships (base→derived type IDs) and implement “child-of” queries by checking those sets.

## Phase 4: Pointer-free serialization support (explicit snapshots, not pointer graphs)

Instead of serializing `Entity` directly (which tends to pull in storage internals and pointers), provide explicit snapshot helpers.

### Snapshot concept (game chooses what to persist)

A snapshot is a POD-friendly structure:

- entities: `(EntityHandle or EntityID, tags, entity_type, cleanup?)`
- per component type `T`: list of `(EntityHandle or EntityID, T value)`

Games specify the component list explicitly:

- `snapshot<Transform, Health, Inventory>(...)`

This avoids “unknown component type” issues and avoids pointer-linking entirely.

### Reference fields inside components

If components contain relationships, they store:

- `EntityHandle` for entity refs
- other stable handles/IDs for non-entity resources

No `Entity*` or smart pointers in serialized state.

## Practical performance checklist (so we don’t regress games)

If we adopt handles + pools, performance should improve, but only if we avoid these traps:

- Don’t keep `getEntityForID` as linear scan.
- Don’t run queries twice (`has_values` + `gen_first`).
- Don’t rebuild intermediate vectors unnecessarily.
- Don’t do debug logging in release hot paths.
- Avoid `shared_ptr` for the primary iteration list if possible; if kept, limit refcount churn.

## Suggested implementation order (lowest risk first)

1) **Land handle API + O(1) ID lookup** (fixes real perf cliff; low user-facing change).
2) **Fix `EntityQuery` to early-exit + avoid double-run** (pure perf, no public API break).
3) **Document + migrate pointer-shaped game fields to `EntityHandle`** (kills pointer serialization needs in game state).
4) **Introduce component pools behind existing `Entity` API** (big internal refactor; large perf upside).
5) **Add optional snapshot helpers** (nice UX for saves/network, pointer-free by design).

## How this addresses “remove serializing pointers”

- Game state no longer stores pointer values; it stores `EntityHandle` (POD) and component values.
- ECS no longer encourages pointer graphs via per-component `unique_ptr` allocations.
- Save/network formats no longer need pointer-linking contexts; they become deterministic, explicit, and portable.

