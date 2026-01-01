# Snapshot + Apply (pointer-free persistence helpers)

This library provides a **pointer-free snapshot surface** intended to make save/load,
replay, and debugging easier.

It does **not** dictate how you serialize data or how you identify entities across runs.
Instead, it provides:

- `snapshot_for<T>(projector)` to export pointer-free data keyed by `EntityHandle`
- `apply_snapshot(...)` helpers to restore data by resolving handles
- compile-time guards to prevent pointer-like types from entering snapshot values

## Concepts

- **`EntityHandle`**: a stable (slot, generation) handle for *this run*. Handles become
  unresolvable when entities are deleted and slots are reused (generation mismatch).
- **Pointer-free values**: snapshot values (or projected DTOs) must be free of raw/smart
  pointers and similar pointer-like wrappers (see `src/core/pointer_policy.h`).
- **Per-component shape**: snapshots are fundamentally **per component type** (one list
  of `(EntityHandle, Value)` pairs per `T`). A convenience “world snapshot” wrapper exists
  to bundle multiple component snapshots together.

## Snapshot (export)

### Direct component snapshot (component must be copyable)

```cpp
// Returns std::vector<std::pair<EntityHandle, T>>
auto snap = afterhours::snapshot_for<MyCopyableComponent>();
```

### Projected snapshot (recommended)

Most ECS components are not meant to be trivially copyable/serializable. Prefer projecting
components into a small pointer-free DTO:

```cpp
struct SavePosition {
  float x = 0;
  float y = 0;
};

auto snap = afterhours::snapshot_for<Position>([](const Position& p) {
  return SavePosition{ .x = p.x, .y = p.y };
});
// snap: std::vector<std::pair<EntityHandle, SavePosition>>
```

**Important:** the projected DTO type must be:
- copy-constructible
- pointer-free (no raw pointers, smart pointers, reference_wrappers, etc.)

## Apply (import/restore)

Apply takes a snapshot vector and an **applier** callback:

```cpp
afterhours::ApplySnapshotOptions opts{};
opts.missing_entity_policy = afterhours::ApplySnapshotOptions::MissingEntityPolicy::Skip;

auto res = afterhours::apply_snapshot<SavePosition>(
  snap,
  [](afterhours::Entity& e, const SavePosition& v) {
    if (!e.has<Position>()) e.addComponent<Position>();
    e.get<Position>().x = v.x;
    e.get<Position>().y = v.y;
  },
  opts
);
```

### Missing-entity policy (handles that don’t resolve)

When `EntityHelper::resolve(handle)` fails, you control what happens:

- **`Skip` (default)**: counts `skipped_unresolved++`, does not call applier.
- **`Create`**: spawns a new entity and calls applier on it (counts `spawned++`).
- **`Error`**: records an error (counts `errors++`, sets `first_error`), does not apply/spawn.

```cpp
opts.missing_entity_policy = afterhours::ApplySnapshotOptions::MissingEntityPolicy::Error;
auto res = afterhours::apply_snapshot<MyDTO>(snap, apply_fn, opts);
if (res.errors > 0) {
  // res.first_error contains the first handle that failed to resolve
}
```

### Custom entity creation (recommended when using Create)

If you want “create” to mean something specific (permanent entities, tags, setup, etc.),
use the overload that takes a `create_entity()` callback:

```cpp
opts.missing_entity_policy = afterhours::ApplySnapshotOptions::MissingEntityPolicy::Create;

auto res = afterhours::apply_snapshot<MyDTO>(
  snap,
  apply_fn,
  []() -> afterhours::Entity& {
    // customize as needed:
    return afterhours::EntityHelper::createEntity();
    // or: return afterhours::EntityHelper::createPermanentEntity();
  },
  opts
);
```

### Convenience: apply direct component snapshots

If you used `snapshot_for<T>()` (no projector), you can use:

```cpp
auto snap = afterhours::snapshot_for<MyCopyableComponent>();
auto res  = afterhours::apply_snapshot_for<MyCopyableComponent>(snap);
```

## “World snapshot” convenience (bundle multiple component snapshots)

Underlying shape remains **per-component**, but you can bundle multiple component snapshots:

```cpp
auto world = afterhours::snapshot_world<Position, Velocity>();

afterhours::ApplySnapshotOptions opts{};
opts.missing_entity_policy = afterhours::ApplySnapshotOptions::MissingEntityPolicy::Error;

auto res = afterhours::apply_world_for<Position, Velocity>(world, opts);
// res aggregates counts across all component snapshots
```

## Pointer-free policy notes

- The policy is a **top-level type check** for the snapshot value type.
  It does not recursively inspect member fields.
- If you need stricter guarantees, consider adding an opt-in annotation/trait or
  implementing a recursive checker (more work).

## Compile-fail regression test (pointer-free enforcement)

There is a compile-fail test that must **fail to compile** when a snapshot projector
returns a pointer-like type:

- `compile_fail/snapshot_for_pointer_like_projected_value.cpp`

Run the check:

```bash
bash ./check_compile_fail_tests.sh
```

This is also invoked by:

```bash
./check_plugin_boundaries.sh
```

