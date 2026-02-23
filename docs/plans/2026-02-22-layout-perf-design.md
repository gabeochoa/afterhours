# Layout Engine Performance Optimization

Incremental improvements to the UI layout engine (`autolayout.h`, `systems.h`), informed by studying [Clay](https://github.com/nicbarker/clay) and [RAD Debugger](https://github.com/EpicGamesExt/raddebugger)'s UI systems.

**Constraint:** Keep the existing recursive multi-pass structure, `ComponentConfig` builder API, and ECS integration. No API breakage for game code. Speed is key while maintaining the structure of the code.

**Goal:** Make scrolling feel smooth; reduce per-frame layout cost generally.

---

## Reference Analysis

### Clay (nicbarker/clay)
- Single-header C layout library. Zero heap allocation — all flat pre-allocated arrays.
- Layout is a single BFS pass after tree construction. Sizing uses 4 types: FIT (shrink-to-content), GROW (fill remaining), FIXED, PERCENT.
- Compression uses largest-first leveling: find the largest child, shrink it toward the second-largest, repeat. Converges in one pass without iteration.
- Scroll is purely a render-time offset (`scrollPosition` applied via `childOffset` in the clip config). Layout does not re-run when only scroll position changes.
- The sidebar scrolling example creates 100 items and prints layout time in microseconds.

### RAD Debugger (EpicGamesExt/raddebugger)
- C UI library with retained box tree and immediate-mode API. Arena allocators — zero per-frame heap allocation.
- Layout runs 5 passes per axis independently: standalone → upwards-dependent → downwards-dependent → enforce constraints → position.
- Constraint enforcement distributes violations proportional to `(1 - strictness)` in a single pass — no iteration.
- Scroll uses a two-level `view_off_target` / `view_off` system with exponential smoothing: `view_off += rate * (target - current)`.
- Boxes persist across frames via hash map, enabling smooth animation interpolation on `hot_t`, `active_t`, `position_delta`.

### Current Afterhours System
- 7 recursive passes: reset → standalone → parent-dependent → child-dependent → solve violations → relative positions → rect bounds.
- `solve_violations` iterates up to 10 times per container trying to converge. Separate `tax_refund` handles expansion.
- Entity lookups via `std::map<EntityID, RefEntity>` — O(log n) per lookup, ~17 `to_cmp()` call sites in autolayout.h.
- Per-frame `std::vector` allocations in `solve_violations`, `tax_refund`, `compute_relative_positions` for `layout_children`.
- `std::function` closures in `for_each_spacing` / `write_each_spacing` — heap allocation per call.
- Scroll changes trigger full layout re-run every frame.

---

## Phase A: Eliminate Per-Frame Allocations and Map Lookups

Internal-only changes. No layout behavior modifications.

### A1. Replace `std::map<EntityID, RefEntity>` with flat vector indexed by EntityID

`AutoLayout::mapping` is `std::map<EntityID, RefEntity>`, and `UIEntityMappingCache::components` is also a `std::map`. Replace both with `std::vector<RefEntity>` indexed by EntityID for O(1) lookup. Use `RefEntity` (reference wrapper) to avoid raw pointers.

**Change all 6+ call sites** that build and pass the map:
- `UIEntityMappingCache::components` in `systems.h`
- `AutoLayout` constructor and `autolayout()` static method in `autolayout.h`
- `force_layout_and_print()` in `utilities.h`
- `autolayout_test.cpp` test harness
- `example/ui/ui_layout/main.cpp` and `example/ui/layout_performance/main.cpp`

**Before:**
```cpp
std::map<EntityID, RefEntity> mapping;
Entity &to_ent(EntityID id) {
    auto it = mapping.find(id);  // O(log n), cache-unfriendly
    return it->second;
}
```

**After:**
```cpp
std::vector<RefEntity> mapping;  // indexed by EntityID, sparse

// Built by BuildUIEntityMapping or callers:
void build_flat_mapping(/* entity query results */) {
    EntityID max_id = 0;
    for (auto &e : entities) max_id = std::max(max_id, e.id);
    mapping.clear();
    mapping.reserve(max_id + 1);
    // Use placement or emplace to populate
}

Entity &to_ent(EntityID id) {
    return mapping[id];  // O(1) array index
}
```

Eliminates O(log n) red-black tree lookups on every `to_cmp()` / `to_ent()` call. Also eliminates the per-frame map construction cost in `BuildUIEntityMapping`.

**Sparse vector concern:** EntityIDs are sequential integers starting from a low base, so the vector won't be excessively sparse. For a 200-element UI tree, the vector will be at most a few hundred entries.

### A2. SmallVector for layout_children in recursive functions

`solve_violations`, `tax_refund`, and `compute_relative_positions` each allocate `std::vector<UIComponent*> layout_children` on the heap per call. For a 200-element tree, this is hundreds of alloc/free cycles per frame. Since these functions are recursive, a single shared scratch buffer would be overwritten by child calls.

Use a stack-allocated small buffer optimization — most containers have fewer than 16 children, so no heap allocation occurs:

```cpp
template<typename T, size_t N>
struct SmallVector {
    std::array<T, N> stack_buf;
    size_t len = 0;
    std::vector<T> heap_buf;  // only used if > N elements

    void push_back(T val) {
        if (len < N) stack_buf[len++] = val;
        else {
            if (heap_buf.empty())
                heap_buf.assign(stack_buf.begin(), stack_buf.begin() + len);
            heap_buf.push_back(val);
            len++;
        }
    }
    T* data() { return len <= N ? stack_buf.data() : heap_buf.data(); }
    size_t size() const { return len; }
    T& operator[](size_t i) { return data()[i]; }
    T* begin() { return data(); }
    T* end() { return data() + len; }
};
```

Note: `UIComponent*` pointers here are internal/local to each function call — they are never stored or exposed, just used as iteration handles within the layout pass.

### A3. Template the spacing callbacks to eliminate `std::function` heap allocs

`for_each_spacing` and `write_each_spacing` take `std::function<float(UIComponent&, Axis)>` — each call allocates a closure on the heap. Called 7 times per layout pass (3x in `calculate_standalone`, 2x in `calculate_those_with_parents`, plus internal calls). Replace with templates:

```cpp
template<typename Fn>
void write_each_spacing(UIComponent &widget, UIComponent::AxisArray<float, 6> &computed, Fn &&cb) {
    computed[Axis::top] = cb(widget, Axis::top);
    computed[Axis::left] = cb(widget, Axis::left);
    computed[Axis::bottom] = cb(widget, Axis::bottom);
    computed[Axis::right] = cb(widget, Axis::right);
    computed[Axis::X] = computed[Axis::left] + computed[Axis::right];
    computed[Axis::Y] = computed[Axis::top] + computed[Axis::bottom];
}

template<typename Fn>
std::array<float, 4> for_each_spacing(UIComponent &widget, Fn &&cb) {
    return {cb(widget, Axis::top), cb(widget, Axis::left),
            cb(widget, Axis::right), cb(widget, Axis::bottom)};
}
```

No heap allocation, and the compiler can inline the callback body.

---

## Phase B: Single-Pass Violation Solving

Replace the iterative `solve_violations` + `tax_refund` with a single-pass distribution algorithm. Layout output will be close to current behavior but not identical — validate with E2E tests and regenerate baselines afterward.

### B1. Merge solve_violations and tax_refund into distribute_space

Current flow:
1. `solve_violations` loops up to 10 times, calling `_solve_error_optional` and `fix_violating_children` each iteration
2. `fix_violating_children` distributes error evenly across resizable children, with a `(1 - strictness)` factor
3. Strictness is mutated down by 0.05 each iteration as a convergence hack
4. If leftover space remains (underflow), `tax_refund` distributes it to Expand or strictness=0 children, recursing into those children

New flow (single function, one pass):

```
distribute_space(widget, axis, is_main_axis):
    available = widget.computed[axis] - widget.computed_padd[axis] - gap_total
    total_children_size = sum(child.computed[axis] for non-abs, non-hidden children)
    error = total_children_size - available

    if error > 0 (overflow — need to shrink):
        // RAD approach: distribute proportional to (1 - strictness) * child_size
        // Larger children absorb proportionally more error than smaller ones
        total_shrinkable = sum(child.computed[axis] * (1 - child.strictness))
        if total_shrinkable > 0:
            ratio = min(1.0, error / total_shrinkable)
            for each child:
                shrink = child.computed[axis] * (1 - child.strictness) * ratio
                child.computed[axis] = max(0, child.computed[axis] - shrink)

    else if error < 0 (underflow — space to distribute):
        remaining = abs(error)
        // First: distribute to Expand children by weight
        total_weight = sum(child.desired[axis].value for Expand children)
        if total_weight > 0:
            for each Expand child:
                child.computed[axis] = remaining * (child.weight / total_weight)
        else:
            // Fallback: distribute to strictness=0 children equally
            for each strictness=0 child:
                child.computed[axis] += remaining / count
                // Recurse into child to propagate refund downward
                distribute_space(child, axis, ...)

    apply_size_constraints(widget)
    for each child:
        apply_size_constraints(child)
        distribute_space(child, ...)
```

### B2. Why this converges in one pass

The current iterative approach exists because `fix_violating_children` distributes error evenly across resizable children, but some children may already be at their minimum size, so the error isn't fully absorbed, requiring another iteration. The strictness mutation (decrement by 0.05 each round) is a hack to force convergence.

RAD avoids this by distributing proportional to `(1 - strictness) * child_size`, which means children that can shrink more absorb more of the error. The proportional distribution is self-balancing — no iteration needed.

### B3. Behavioral differences from current implementation

- **Overflow distribution**: Current distributes evenly then applies strictness factor. New distributes proportional to `child_size * (1 - strictness)`. Larger children shrink more.
- **Strictness mutation**: Current reduces strictness by 0.05 per iteration. New does not mutate strictness (unnecessary with single-pass).
- **Underflow recursion**: Preserved — `tax_refund`'s recursive propagation into strictness=0 children is kept in the new flow.

These differences mean some layouts will shift by a few pixels. Validate with full E2E test suite and regenerate baselines.

---

## Phase C: Skip Layout for Scroll-Only Changes

Make scrolling feel smooth by avoiding full layout re-runs when only the scroll offset changed.

### C1. Why layout can be skipped during scroll

Scroll offset is already applied at render time in `rendering.h` (subtracting `scroll_offset` from child positions). Hit-testing in `detail::apply_scroll_offset` also reads `scroll_offset` directly. Neither depends on `RunAutoLayout` having run — they read the offset from the `HasScrollView` component.

`FixScrollViewPositions` repositions children sequentially for hit-testing, but those positions are relative to the scroll container and don't change when only the offset changes.

### C2. The immediate-mode challenge

In the immediate-mode architecture, UI entities are recreated every frame. This means a naive dirty flag would be set every frame by default.

The key insight: even though entities are recreated, their *structure* (count, sizing modes, text content) is identical frame-to-frame when nothing meaningful changed. We need to detect "same tree as last frame."

### C3. Cheap tree identity check

Compare a lightweight fingerprint of the UI tree across frames. If it matches, skip layout.

```cpp
struct UILayoutFingerprint : BaseComponent {
    size_t entity_count = 0;
    size_t structure_hash = 0;  // hash of child counts + sizing dims
    bool layout_valid = false;
};
```

Computed during `BuildUIEntityMapping` (which already iterates all UI entities):
- `entity_count`: number of UI entities
- `structure_hash`: combine each entity's `children.size()`, `desired[X].dim`, `desired[Y].dim`, `desired_padding` dims, and label length

If both match the previous frame, set `layout_valid = true` and skip `RunAutoLayout`.

This is deliberately conservative — any structural change triggers a full re-layout. The main case it optimizes is scroll-only frames where the tree is rebuilt identically.

### C4. Guard RunAutoLayout with the fingerprint

```cpp
struct RunAutoLayout : System<AutoLayoutRoot, UIComponent> {
    void for_each_with(Entity &entity, AutoLayoutRoot &, UIComponent &cmp, float) override {
        auto *fp = EntityHelper::get_singleton_cmp<UILayoutFingerprint>();
        if (fp && fp->layout_valid) return;  // skip layout entirely

        // ... existing layout code ...
    }
};
```

### C5. What to include in the hash

Must hash (changes require re-layout):
- Entity count
- Each entity's `desired[X].dim`, `desired[Y].dim`, `desired[X].value`, `desired[Y].value`
- `desired_padding` and `desired_margin` dims/values
- `children.size()` per entity
- Label text length (text content changes affect `Dim::Text` sizing)
- `flex_direction`, `flex_wrap`, `justify_content`, `align_items`

Must NOT hash (changes don't affect layout):
- `scroll_offset` (render-time only)
- Focus/hot/active state
- Mouse position
- Background color, text color, border color

### C6. Risk and fallback

If the hash is too aggressive (skips layout when it shouldn't), layouts will be stale. Mitigation:
- Start with a very conservative hash (include everything structural)
- Add a debug toggle to force layout every frame for comparison
- The hash runs inside `BuildUIEntityMapping` which already iterates all entities, so it adds O(n) work to an O(n) pass — minimal overhead

---

## Implementation Order

1. **Phase A** (A1 → A3) — Mechanical changes, remove allocation noise from profiling. Lowest risk. ~2-3 hours.
2. **Phase C** (C1 → C6) — Biggest user-visible win for scroll feel. Moderate risk (hash correctness). ~3-4 hours.
3. **Phase B** (B1 → B3) — Algorithmic improvement for complex layouts. Highest risk (behavior change). ~2-3 hours + baseline regeneration.

## Validation

- **Phase A**: Run full E2E test suite. Output must be identical (no baseline regeneration needed).
- **Phase B**: Run full E2E test suite. Expect some pixel-level diffs. Visual review + regenerate baselines.
- **Phase C**: Run full E2E test suite with scroll-heavy screens. Toggle debug mode to compare layout-every-frame vs. fingerprint-skip. Verify no visual differences.

## Alternatives Rejected

- **Full rewrite to Clay-style flat arrays**: Would require abandoning the ECS-based UI tree. Too invasive for incremental improvement.
- **Per-axis independent passes (RAD style)**: Clean architecture but requires restructuring all 7 passes. Better suited for a future major overhaul.
- **Box persistence / hash caching (RAD style)**: Valuable for animation smoothing but orthogonal to layout performance. Consider as separate work.
- **Replacing `std::map` only inside AutoLayout (keeping map as input)**: Saves the lookup cost but not the map construction cost. Since all call sites are internal, replacing the map everywhere is worth the extra effort.

## Scroll Behavior Note

macOS trackpad generates synthetic momentum scroll events after finger lift. The current system applies these directly (`scroll_offset += delta * scroll_speed`), which works but gives no control over the deceleration curve. Clay handles this by applying wheel deltas with a `* 10` multiplier and having its own momentum system (`scrollMomentum *= 0.95f` decay). RAD uses a target/current split (`view_off += rate * (target - current)`). Either pattern could be adopted independently of these layout performance changes.
