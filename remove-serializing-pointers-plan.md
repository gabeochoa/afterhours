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

## Validation: example-driven tests per phase

Below are concrete test ideas (2–3 per phase) that fit the existing `example/tests` Catch2 setup and the usage patterns in `example/`.

### Phase 1 — Handles + O(1) ID resolution

- **Handle lifecycle correctness**
  - Create entity → merge → get handle → resolve(handle) returns same entity ID.
  - Mark for cleanup + cleanup → old handle resolves invalid.
  - Create a new entity → verify slot reuse bumps generation (old handle stays invalid, new handle valid).
- **`EntityID` lookup correctness under churn**
  - Create N entities, merge, delete a subset (including middle element to force swap-remove), then:
    - `getEntityForIDEnforce(id)` succeeds for survivors
    - `getEntityForID(id)` returns empty for deleted
  - This validates that the ID→slot mapping stays correct across swap-removes.
- **Temp entity semantics preserved**
  - Create entity (temp), confirm query misses it unless forced merge (current behavior).
  - If a “handles-on-create” define exists, ensure resolve(handle) works even when query misses temp.

### Phase 2 — Query behavior (no double-run, correct results)

- **`gen_first` and `has_values` correctness**
  - Build a small world with known entities.
  - Assert:
    - `q.has_values()` matches `(q.gen_count() > 0)`
    - `q.gen_first_enforce().id` equals the first element returned by `q.gen()` for stable-order queries
- **Force-merge behavior stays correct**
  - Create entities without calling `merge_entity_arrays()`.
  - Assert:
    - query with default options misses them
    - query with `{.force_merge=true}` sees them
- **Stop-on-first option actually short-circuits (instrumented)**
  - Add a test-only `whereLambda` that increments a counter on each entity visited.
  - Call `gen_first()` and assert the counter is “small” (ideally 1) in a known ordering case.
  - This requires Phase 2’s implementation to truly early-exit.

### Phase 3 — Game components use IDs/handles (no pointers in persisted state)

- **Reference component uses handles, not pointers**
  - Define a component `Targets { EntityHandle target; }`.
  - Create A and B, set `A.Targets.target = handle_for(B)`.
  - Delete B, then assert resolve(A.Targets.target) is invalid.
- **No pointer serialization policy enforcement (compile-time)**
  - Add a small template in a “snapshot” header that `static_assert`s that snapshot inputs do not contain banned pointer types, e.g.:
    - reject `T*`, `std::unique_ptr`, `std::shared_ptr`
  - Tests can validate the “good” component compiles and (optionally) that a “bad” component is rejected (this may be kept as a commented “negative compile test” example if your CI can’t do compile-fail tests).

### Phase 4 — Component pools/SoA behind existing `Entity` API

- **Core API parity**
  - For a component `Transform`:
    - `addComponent<Transform>` then `has<Transform>` true and `get<Transform>` returns expected values
    - `removeComponent<Transform>` then `has<Transform>` false
- **Swap-remove correctness inside pools**
  - Add `Transform` to many entities.
  - Remove `Transform` from one in the middle.
  - Assert all remaining entities still return correct `Transform` data (validates sparse/dense index updates).
- **EOF reference stability (opt-in)**
  - Under `AFTER_HOURS_KEEP_REFERENCES_UNTIL_EOF`:
    - take a reference/pointer to a component, trigger removals that would normally swap-remove, and assert the reference remains usable until the explicit EOF flush.
  - Without the define:
    - document that the same pattern is unsupported (test should not rely on it).

### Phase 5 — Pointer-free snapshots (explicit serialization surface)

- **Snapshot contains only IDs/handles**
  - Build a tiny world, take `snapshot<Transform, Health>()`.
  - `static_assert` snapshot types are pointer-free:
    - no raw pointers/smart pointers in snapshot structs
    - handles are POD (`EntityHandle`)
- **Round-trip correctness (in-memory)**
  - Serialize snapshot to a buffer (format doesn’t matter yet), deserialize, rebuild world, and assert:
    - entity count matches
    - selected component values match
    - entity references via handles resolve correctly after rebuild (where applicable)

## “Run non-raylib tests between each phase” (current baseline)

Recommended non-raylib targets to run after each phase:

- `make -C example/tests`
- `make -C example/tag_filters`
- `make -C example/basic_usage`
- `make -C example/custom_queries`
- `make -C example/simple_system`
- `make -C example/functional_usage`
- `make -C example/not_system`

Notes from reviewing/running `example/` in this environment:

- `example/tests` currently passes.
- `example/ui_layout` currently runs but has 2 failing assertions due to float rounding (`87.5` vs expected `88`). It’s useful coverage, but it shouldn’t gate phases until it’s made epsilon/rounding-tolerant.

