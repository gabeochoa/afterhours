# RFC: Entity Tags/Groups integrated into Entity and EntityQuery (afterhours)

## Summary
Introduce lightweight bitset-based tags on `afterhours::Entity` and integrate tag predicates into `EntityQuery`:
- Simplify common filters and enable bulk operations
- Replace ad-hoc flags like `.include_store_entities()` and marker-only components
- Keep the feature generic in `vendor/afterhours`; games define their own typed tag enums

Backwards-compatible: adds a small field to `Entity`; optional serialization is guarded.

## Motivation
Current cross-cutting filters (store vs non-store, permanent, round cleanup) are scattered across ad-hoc flags and marker components. Centralizing this with tags provides:
- Clear, composable semantics (all/any/none)
- Fast, constant-time checks
- Straightforward bulk ops (e.g., mark all tagged for cleanup)

## Goals
- Generic tag mechanism in `afterhours` (no game-specific names)
- Fast tag membership checks
- Ergonomic AND/OR/NOT tag filters via `EntityQuery` methods
- Backwards-compatible, opt-in serialization

## Non-goals
- A global type-safe registry in the library (games keep their own enums)
- Replacing `entity.cleanup` (tags are membership, not one-shot actions)

## Design

### Entity: Tag storage and API (in vendor/afterhours)
- Data
  - `TagBitset tags{}` stored on `afterhours::Entity`
  - Capacity via compile-time constant: `AFTER_HOURS_MAX_ENTITY_TAGS` (default 64)
- Types
  - `using TagId = uint8_t;`
  - `using TagBitset = std::bitset<AFTER_HOURS_MAX_ENTITY_TAGS>;`
- Methods
  - `void enableTag(TagId id);`
  - `void disableTag(TagId id);`
  - `bool hasTag(TagId id) const;`
  - `bool hasAllTags(const TagBitset &mask) const;`
  - `bool hasAnyTag(const TagBitset &mask) const;`
  - `bool hasNoTags(const TagBitset &mask) const;`

Example (library side):
```cpp
#ifndef AFTER_HOURS_MAX_ENTITY_TAGS
constexpr size_t AFTER_HOURS_MAX_ENTITY_TAGS = 64;
#endif

using TagId = uint8_t;
using TagBitset = std::bitset<AFTER_HOURS_MAX_ENTITY_TAGS>;

struct Entity {
  // existing fields...
  TagBitset tags{};

  void enableTag(TagId id) { if (id < AFTER_HOURS_MAX_ENTITY_TAGS) tags.set(id); }
  void disableTag(TagId id) { if (id < AFTER_HOURS_MAX_ENTITY_TAGS) tags.reset(id); }
  bool hasTag(TagId id) const { return id < AFTER_HOURS_MAX_ENTITY_TAGS && tags.test(id); }

  bool hasAllTags(const TagBitset &m) const { return (tags & m) == m; }
  bool hasAnyTag(const TagBitset &m) const { return (tags & m).any(); }
  bool hasNoTags(const TagBitset &m) const { return (tags & m).none(); }
};
```

### Query integration (extend EntityQuery)
Add tag-aware predicates alongside existing component filters. Suggested API:
```cpp
struct EntityQuery {
  // ... existing
  EntityQuery &whereHasTag(TagId id);
  EntityQuery &whereHasAllTags(const TagBitset &mask);
  EntityQuery &whereHasAnyTag(const TagBitset &mask);
  EntityQuery &whereHasNoTags(const TagBitset &mask);
};
```
These are implemented as `Modification`s that invoke the `Entity` tag predicates during partitioning.

Note: deprecate `.include_store_entities(bool)` in favor of explicit tag predicates. It is not currently consulted during filtering and can be removed after migration.

### Optional typed sugar (outside library)
Games can provide typed helpers without changing the core API:
```cpp
enum struct GameTag : uint8_t { Store, Permanent, CleanupOnRoundEnd /* ... */ };

inline TagBitset make_mask(std::initializer_list<GameTag> list) {
  TagBitset m; for (auto t : list) m.set(static_cast<uint8_t>(t)); return m;
}
```

### Optional bulk ops (EntityHelper)
Consider small helpers that use tag masks for common operations:
- `mark_all_for_cleanup_with_tags(const TagBitset &mask)`
- `erase_all_with_tags(const TagBitset &mask)`

These are opt-in and can be omitted if we prefer to keep `EntityHelper` minimal.

### Serialization (optional)
If a consumer enables bitsery-based serialization of `Entity`, include `tags` next to `componentSet` under a guard. Suggested macro: `AFTER_HOURS_ENABLE_BITSERY_SERIALIZE`.
```cpp
s.ext(entity.componentSet, bitsery::ext::StdBitset{});
s.ext(entity.tags, bitsery::ext::StdBitset{}); // NEW
s.value1b(entity.cleanup);
```

## Examples (with EntityQuery)

### Exclude store by default in a helper
```cpp
auto ents = afterhours::EntityQuery()
  .whereHasNoTags(make_mask({GameTag::Store}))
  .gen();
```

### Include store for specific queries
```cpp
auto ents = afterhours::EntityQuery()
  .whereHasAnyTag(make_mask({GameTag::Store}))
  .whereHasComponent<IsFloorMarker>()
  .gen();
```

### Bulk cleanup at round end
```cpp
for (auto &e : afterhours::EntityQuery()
                  .whereHasAllTags(make_mask({GameTag::CleanupOnRoundEnd}))
                  .gen()) {
  e.get().cleanup = true;
}
```

### Protect permanent entities during wipe
```cpp
for (auto &e : afterhours::EntityQuery()
                  .whereHasNoTags(make_mask({GameTag::Permanent}))
                  .gen()) {
  // eligible for full wipe
}
```

## Performance
- Tag checks are constant-time bitset ops
- Query remains O(N) over live entities; tag checks add negligible cost
- If needed, we can explore optional per-tag indices after profiling

## Migration Plan (consumer repo)
1. Add `GameTag` enum and helpers
2. Tag entities at creation/modification sites
3. Replace `.include_store_entities()` and marker-only filters with tag predicates
4. Keep `entity.cleanup` unchanged; tags are for membership and bulk ops
5. Optionally unify existing “permanent” semantics with a `Permanent` tag

## Alternatives Considered
- Only marker components: works but spreads logic and adds boilerplate
- String-based tags: flexible but slower and error-prone
- Tags stored outside `Entity`: complicates ownership/serialization

## Open Questions
- Capacity: is 64 sufficient? Do we want to permit 128 via macro override?
- Should we add bulk ops to `EntityHelper` or keep them in consumers?
- Do we want library-provided typed helpers, or leave them entirely to consumers?

## Backwards Compatibility
- New entities default to zeroed tag bitset
- Saves without tags load with `tags = 0` when serialization is enabled

## Rollout
- Add `tags` field and tag predicates to `Entity`
- Extend `EntityQuery` with tag-aware predicates
- Deprecate and later remove `.include_store_entities()` after call sites migrate