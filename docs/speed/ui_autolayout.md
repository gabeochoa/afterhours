# UI / AutoLayout Plugin

1. **HIGH — Avoid per-frame `EntityQuery` scans for UI entities; maintain a persistent UI entity list.**
   `BuildUIEntityMapping` runs a full `EntityQuery` every frame to rebuild the `UIEntityMappingCache`. Instead, maintain the list incrementally (add on create, remove on destroy).

2. **HIGH — Replace `std::map<EntityID, RefEntity>` in `UIEntityMappingCache` with a flat hash map or direct array.**
   `std::map` is a red-black tree. Entity IDs are integers; a hash map or sparse array is O(1).

3. **HIGH — Cache autolayout results and only re-run when the UI tree changes (dirty flag).**
   `AutoLayout::autolayout()` runs every frame for every root. Add a dirty flag system so unchanged subtrees skip layout.

4. **HIGH — Avoid repeated `to_cmp()` / `to_ent()` lookups in recursive layout functions.**
   Every layout function calls `this->to_cmp(child_id)` which does a map lookup. Pass references down the recursion instead.

5. **HIGH — Reduce the number of recursive passes in autolayout.**
   Currently: `reset_computed_values` -> `calculate_standalone` -> `calculate_those_with_parents` -> `calculate_those_with_children` -> `solve_violations` -> `compute_relative_positions` -> `compute_rect_bounds`. That's 7 passes over the tree. Combine passes where possible.

6. **MED — Use arena allocation for UI entities instead of `shared_ptr`.**
   UI entities are created/destroyed every frame in immediate mode. Arena allocation eliminates per-entity heap allocation.

7. **MED — Replace `std::map<UI_UUID, EntityID>` in `existing_ui_elements` with a flat hash map.**
   `std::map` has O(log N) lookups. This is hit for every UI element every frame.

8. **MED — Avoid `std::stringstream` + `std::hash<string>` for UI element hashing in `mk()`.**
   `mk()` creates a `stringstream`, writes to it, converts to string, then hashes. Pre-compute a numeric hash from the source location directly (combine file hash + line + column + parent id).

9. **MED — Cache `EntityHelper::get_singleton_cmp<UIContext>()` lookups per frame.**
   Multiple systems call this every frame. Cache the pointer in `once()`.

10. **MED — Skip hidden subtrees entirely in layout passes.**
    `should_hide` children still get recursed into during layout. Skip the entire subtree.

11. **MED — Use `std::vector<UIComponent*>` for children instead of `std::vector<EntityID>`.**
    Avoids the `to_cmp(child_id)` map lookup for every child access during layout.

12. **MED — Pool-allocate `RenderInfo` commands or use a ring buffer.**
    `context.render_cmds` grows/shrinks every frame.

13. **MED — Sort render commands once using radix sort instead of `std::sort`.**
    Layer values are small integers; radix sort is O(N).

14. **MED — Avoid `find_clip_ancestor` / `find_scroll_view_ancestor` walking up the tree per-entity.**
    Cache the clip ancestor on the UIComponent during layout.

15. **MED — Avoid `compute_effective_opacity` walking up the tree per-entity during rendering.**
    Compute and cache cumulative opacity during layout.

16. **MED — Use `SmallVector<EntityID, 8>` for UIComponent::children.**
    Most UI elements have < 8 children. Avoids heap allocation.

17. **MED — Batch text measurement calls; avoid redundant measure_text for the same string.**
    The `TextMeasureCache` helps but `position_text_ex` still calls `measure_text` multiple times (binary search).

18. **LOW — Avoid `fmt::format` in layout warning messages unless actually logging.**
    `get_child_debug_name` / `get_parent_debug_name` create `fmt::format` strings even when the warning isn't triggered.

19. **LOW — Use `constexpr` for layout constants (GRID_UNIT_720P, BASE_WRAP_TOLERANCE, etc.).**

20. **LOW — Replace `std::bitset<4>` for corner settings with a plain `uint8_t` and bit ops.**

21. **LOW — Avoid `std::optional<TextStroke>` / `std::optional<TextShadow>` in rendering hot path; use null sentinels.**

22. **LOW — Pre-compute `fetch_screen_value_` once per layout call instead of per-widget.**

23. **LOW — Avoid `std::array<float, 4>` temporaries in `for_each_spacing`; write directly.**

24. **LOW — Use `memset` to reset computed values instead of per-field assignment.**

25. **LOW — Avoid `std::source_location` overhead in release builds for `mk()`.**

26. **LOW — Profile the `solve_violations` loop (up to 10 iterations) and tune convergence.**

27. **LOW — Use `[[likely]]` for the common case of `!child.absolute && !child.should_hide`.**

28. **LOW — Cache `font_manager.get_active_font()` per frame instead of per-text-draw.**

29. **LOW — Avoid `push_rotation` / `pop_rotation` calls when rotation is 0.**

30. **LOW — Use hardware scissor test batching to reduce state changes.**

31. **LOW — Profile `draw_rectangle_rounded` segment count and reduce for small elements.**

32. **LOW — Avoid checking `entity.has<X>()` repeatedly in `render_me`; combine into a single bitmask check.**

33. **LOW — Use `string_view` for `HasLabel::label` to avoid string copies.**

34. **LOW — Avoid `RenderCommandBuffer` re-creation every frame; reuse and clear.**

35. **LOW — Move the bubble-sort fallback (`#if __WIN32`) to use `std::sort` on modern MSVC.**
