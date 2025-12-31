# Remove “serializing pointers” from Afterhours (plan + phases)

This plan removes the need to serialize **any** pointers (raw pointers, smart pointers, observed-pointer systems) by making persisted state strictly **IDs/handles**, while keeping the public ECS API mostly intact and avoiding performance regressions.

## Existing setup

### ECS component storage (pointer-based)

- `Entity` stores components as heap objects:
  - `componentArray`: `std::array<std::unique_ptr<BaseComponent>, max_num_components>`
  - `componentSet`: bitset presence mask keyed by `ComponentID`

This implies:

- **runtime**: lots of small allocations and pointer chasing
- **serialization pressure**: anything that tries to serialize “an Entity” will quickly run into pointer-shaped state

### Entity storage and access patterns

- Entities live as `std::shared_ptr<Entity>` in a dense list plus a temp list that is merged later.
- `EntityHelper::getEntityForID` is currently a linear scan (O(N)).

### Existing usage patterns (from `example/`)

The UI stack already follows the “good” pattern:

- relationships are modeled with `EntityID` (parent/children/focus/hot/etc.)
- code resolves IDs back to entities when needed

This is exactly the direction we want for all persisted game state.

### Prior art in this repo: `origin/entity-handle-slots`

That branch introduces the right handle concept:

- `EntityHandle { slot, gen }` (slot+generation)
- `EntityHelper::resolve(handle)` and generation bumps on delete

But it likely felt slow because key hot paths were still expensive:

- `EntityID -> entity` remained O(N)
- `EntityQuery` could run queries more than once per call chain and didn’t early-exit
- `shared_ptr` remained on the iteration hot path

## Current problem

### “No pointers in serialized data” (hard requirement)

Persisted formats (save-game, network snapshots) must contain **zero pointer values**, including:

- raw pointers (`T*`, `void*`)
- smart pointers (`std::unique_ptr`, `std::shared_ptr`)
- pointer-linking/observed-pointer serialization systems (e.g., Bitsery pointer extensions)

Everything persisted must be **IDs or handles**.

### Performance constraints

- We can’t slow down games.
- Ideally, this refactor is a net win (fewer allocations, better locality, faster lookups).

### Compatibility constraint: “stable until end-of-frame”

- Best case: references don’t change until end of the frame.
- Acceptable back-compat: make this behavior opt-in via:
  - `AFTER_HOURS_KEEP_REFERENCES_UNTIL_EOF`

## Solution overview

### Core idea

Separate “identity” from “memory address”:

- **Persist**: IDs/handles only.
- **Runtime access**: resolve IDs/handles through ECS to reach current objects.

### Key building blocks

- **Entity handles**: slot+generation handles that detect stale references after delete/reuse.
- **O(1) resolution**:
  - `EntityID -> slot/handle -> entity` must be O(1) (no scans).
- **Pointer-free serialization primitives**:
  - explicit snapshot structures that serialize `(handle/id, component value)` pairs
  - never serialize `Entity` via pointer graphs
- **(Optional but recommended) component pools**:
  - move from per-entity `unique_ptr<BaseComponent>` to per-component dense pools (SoA-friendly)
  - keep public `Entity::addComponent/get/has/remove` intact by forwarding internally
- **EOF stability mode**:
  - defer destructive operations until a frame boundary when `AFTER_HOURS_KEEP_REFERENCES_UNTIL_EOF` is enabled

## Solution phase breakdown

### Phase 1 — Entity handles + O(1) `EntityID` resolution (no API break)

**Goal**: make “store handle/id, resolve later” cheap enough to be the default pattern everywhere.

- **Add/standardize** `EntityHandle { uint32_t slot; uint32_t gen; }` (POD).
- **Expose**:
  - `EntityHelper::handle_for(const Entity&) -> EntityHandle`
  - `EntityHelper::resolve(EntityHandle) -> OptEntity`
- **Make `EntityID -> entity` O(1)**:
  - Add `std::vector<uint32_t> id_to_slot` (or equivalent).
  - Update it on merge/create/delete.
  - Re-implement `getEntityForID` using this mapping (no scans).
- **Release build performance**:
  - compile out handle validation/logging in hot paths (keep in debug).

### Phase 2 — Fix `EntityQuery` hot paths (avoid double work)

**Goal**: queries should not become the next bottleneck once ID/handle resolution becomes cheap.

- Ensure `gen_first()`/`has_values()` do not run the query twice.
- Implement true early-exit for “stop on first”.
- Avoid building intermediate vectors when only existence/first element is needed.

### Phase 3 — Remove pointer-shaped state from *game* components (policy + migration)

**Goal**: games stop needing pointer-linking serialization immediately, even before internal ECS storage changes.

- **Rule**: serialized components must not contain:
  - `Entity*`, `RefEntity`, `std::shared_ptr<Entity>`, `std::unique_ptr<...>`
- **Replacement**:
  - use `EntityHandle` (preferred) or `EntityID` (only when lifetime is tightly controlled).
- **Typical pattern**:
  - store `EntityHandle target;`
  - use `if (auto e = EntityHelper::resolve(target)) { ... }`

### Phase 4 — Component storage: pools/SoA behind existing `Entity` API

**Goal**: remove internal pointer pressure, reduce allocations, improve locality, and make snapshots straightforward.

- Introduce `ComponentPool<T>` (dense + sparse index):
  - `dense: std::vector<T>`
  - `dense_to_entity: std::vector<EntityID>`
  - `entity_to_dense: std::vector<uint32_t>`
- Keep public API shape:
  - `Entity::addComponent<T>(...)`
  - `Entity::get<T>()`
  - `Entity::has<T>()`
  - `Entity::removeComponent<T>()`

**Compatibility: EOF stability**

- `AFTER_HOURS_KEEP_REFERENCES_UNTIL_EOF`:
  - defer swap-removes/compaction/slot reuse until an end-of-frame flush (or treat `EntityHelper::cleanup()` as the flush boundary).
- Default mode (no define):
  - immediate swap-remove; no pointer stability guarantees.

**Derived/child-of queries**

Current RTTI-based `has_child_of/get_with_child` depends on pointer-backed components.
Options:

- legacy-only support for derived queries, or
- explicit derived registry (base → derived type IDs) if we want it with pools

### Phase 5 — Pointer-free serialization: explicit snapshots (no pointer graphs)

**Goal**: serialization becomes deterministic and pointer-free by design.

- Provide snapshot helpers (game chooses the component list):
  - entities: `(EntityHandle, tags, entity_type, cleanup?)`
  - per component type `T`: `(EntityHandle, T value)`
- Components that reference other entities store:
  - `EntityHandle` (not pointers)

## Performance guardrails (keep games fast)

- No O(N) scans for ID lookups.
- No query double-run patterns (`has_values()` then `gen_first()`).
- No debug logging in release hot paths.
- Avoid `shared_ptr` churn in the core iteration path where possible.

