# Item #1: Dirty-Flag Guard on `merge_entity_arrays()`

## Problem

`SystemManager::tick()` calls `EntityHelper::merge_entity_arrays()` after every update system. With 30+ update systems and 500+ entities, that's 30+ O(N) vector operations per frame, most of which are no-ops (no entities were created since the last merge).

The current implementation has a `if (temp_entities.empty()) return;` early-out, but even that check runs 30+ times per frame. More importantly, the merge loop itself iterates all temp entities, assigns slots, and clears the vector -- all unnecessary when no entities were created.

## Context

- **File:** `src/core/entity_collection.h` lines 213-226
- **Call sites:** After every update system in `SystemManager::tick()` (system.h:463), in `cleanup()`, `delete_all_entities()`, snapshot operations, entity queries with `force_merge`, plugin initialization (files, settings, pathfinding), UI utilities
- **Entity creation:** Happens in `for_each()`, `once()`, and `after()` -- not just between systems
- **Cross-system dependency:** System A creates entities that later system B needs to see in the same frame, so merge must still happen when entities were actually created
- **Multiple collections:** UI uses a separate collection; thread-local collections via `set_default_collection()` exist
- **Cross-thread access:** Multiple threads can access the same `EntityCollection` concurrently
- **Fixed tick gap:** `fixed_tick()` never calls merge (known issue, deferred)

## Design Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Flag type | `std::atomic<bool>` | Cross-thread access to same collection exists |
| Flag location | Member of `EntityCollection` | Per-collection (needed for multiple collections), co-located with `temp_entities` |
| Public API | Expose `has_pending_entities()` | Systems can check if merge is pending |
| Encapsulate `temp_entities` | Yes, add `add_temp_entity()` method | Guarantees flag is always set; deprecate direct access |
| Early-out strategy | Replace `empty()` check with flag-only | Less code, less surface area for bugs |
| Flag semantics | Simple: "something was pushed" | Not "has real work". Rare all-cleanup-entity case not worth optimizing |
| `force_merge` behavior | Bypasses the dirty flag | `force_merge` exists for callers that need guaranteed merge regardless |
| Clear points | `merge_entity_arrays()` + `delete_all_entities()` | Both consume/clear `temp_entities` |
| Perf counters | Behind `#ifdef AFTERHOURS_PERF_COUNTERS` | Track merge calls vs skips per frame |
| Debug logging | Warn when merge called with flag=false | Catches missed setter points during development |

## Changes

### 1. Add dirty flag to `EntityCollection`

**File:** `src/core/entity_collection.h`

Add member:
```cpp
std::atomic<bool> temp_dirty{false};
```

### 2. Add `add_temp_entity()` method

**File:** `src/core/entity_collection.h`

New method on `EntityCollection`:
```cpp
void add_temp_entity(std::shared_ptr<Entity> entity) {
    temp_entities.push_back(std::move(entity));
    temp_dirty.store(true, std::memory_order_release);
}
```

### 3. Deprecate direct `temp_entities` access

**File:** `src/core/entity_collection.h`

Add `[[deprecated("Use add_temp_entity() instead")]]` to direct `temp_entities` member or make it private with a deprecated getter.

### 4. Add `has_pending_entities()` public getter

**File:** `src/core/entity_collection.h`

```cpp
bool has_pending_entities() const {
    return temp_dirty.load(std::memory_order_acquire);
}
```

### 5. Update `createEntityWithOptions()` to use `add_temp_entity()`

**File:** `src/core/entity_collection.h` lines 199-211

Change from:
```cpp
temp_entities.push_back(e);
```
To:
```cpp
add_temp_entity(e);
```

### 6. Update UI direct push to use `add_temp_entity()`

**File:** `src/plugins/ui/utilities.h` line 176

Change from:
```cpp
default_coll.temp_entities.push_back(root_shared);
```
To:
```cpp
default_coll.add_temp_entity(root_shared);
```

### 7. Update `merge_entity_arrays()` to use dirty flag

**File:** `src/core/entity_collection.h` lines 213-226

Change from:
```cpp
void merge_entity_arrays() {
    if (temp_entities.empty())
        return;
    // ... merge loop ...
    temp_entities.clear();
}
```
To:
```cpp
void merge_entity_arrays() {
    if (!temp_dirty.load(std::memory_order_acquire)) {
#ifdef AFTERHOURS_PERF_COUNTERS
        ++merge_skipped_count;
#endif
        return;
    }
    // ... merge loop (unchanged) ...
    temp_entities.clear();
    temp_dirty.store(false, std::memory_order_release);
#ifdef AFTERHOURS_PERF_COUNTERS
    ++merge_executed_count;
#endif
}
```

### 8. Update `delete_all_entities()` to clear flag

**File:** `src/core/entity_collection.h`

After the existing `merge_entity_arrays()` call and entity cleanup, ensure:
```cpp
temp_dirty.store(false, std::memory_order_release);
```

### 9. Add perf counters

**File:** `src/core/entity_collection.h`

```cpp
#ifdef AFTERHOURS_PERF_COUNTERS
int merge_executed_count = 0;
int merge_skipped_count = 0;

void reset_perf_counters() {
    merge_executed_count = 0;
    merge_skipped_count = 0;
}
#endif
```

### 10. Add debug log for flag=false merge calls

**File:** `src/core/entity_collection.h`

In `merge_entity_arrays()`, after the dirty flag check, add:
```cpp
#ifdef AFTER_HOURS_DEBUG
if (!temp_entities.empty() && !temp_dirty.load(std::memory_order_acquire)) {
    log_warn("merge_entity_arrays: temp_entities non-empty but dirty flag was false");
}
#endif
```

## What Does NOT Change

- System ordering and behavior: merge still happens between systems when entities are created
- `force_merge` callers: snapshot and entity query `force_merge` paths bypass the flag (they already check `!collection.get_temp().empty()` themselves)
- Fixed tick: still does not call merge (known issue, deferred)
- Entity creation API: `createEntity()`, `createPermanentEntity()`, `createEntityWithOptions()` signatures unchanged
- Plugin initialization patterns: unchanged (they call merge explicitly after creating singletons)

## Expected Impact

With 30+ update systems and most systems not creating entities on any given frame, the majority of merge calls become a single atomic bool load + branch-not-taken. For 500+ entities, this eliminates ~29 unnecessary O(N) vector scans per frame (assuming ~1 system per frame actually creates entities).

## Testing

- Verify existing entity creation + merge tests still pass
- Verify cross-system entity visibility: system A creates entity, system B (later in same frame) can iterate it
- Verify UI split-collection mode works with `add_temp_entity()`
- Verify `force_merge` paths still work (snapshot, entity query)
- Verify `delete_all_entities()` clears the flag
- Check perf counters show expected skip ratio in a real game frame
