# Pathfinding Plugin

1. **HIGH — Replace `std::set<Vec2>` with a flat hash set or grid-based visited array.**
   `std::set` does red-black tree operations with O(log N) per insert/lookup. A hash set or direct 2D array is O(1).

2. **HIGH — Replace `std::map<Vec2, ScoreValue>` in A\* with flat arrays or hash maps.**
   `g_score` and `f_score` use `std::map` (red-black tree). For grid-based pathfinding, a 2D array is O(1) and cache-friendly.

3. **HIGH — Replace `std::set` open_set in A\* with a priority queue (min-heap).**
   `get_lowest_f` does a linear scan of the open set every iteration. A binary heap gives O(log N) extraction.

4. **HIGH — Use `new Node` allocations in BFS with a pool allocator or arena.**
   BFS allocates `new Node` for every visited cell and deletes them all at the end. An arena allocator eliminates per-node malloc/free overhead.

5. **HIGH — Avoid `std::function` callbacks for `is_walkable`, `distance_fn`, `get_neighbors_fn`.**
   These are called per-node in the inner loop. Template parameters or function pointers eliminate virtual dispatch.

6. **MED — Pre-allocate neighbor vectors in `get_neighbors_fn` instead of creating a new vector each call.**
   The default neighbor function creates a `std::vector<Vec2>` with 8 elements on every call. Use a fixed-size array.

7. **MED — Use Manhattan distance for grid-based heuristics instead of Euclidean.**
   `std::sqrt(dx*dx + dy*dy)` is expensive. Manhattan distance is faster and still admissible for grid movement.

8. **MED — Implement Jump Point Search (JPS) for uniform-cost grids.**
   JPS can be 10-100x faster than A* on uniform grids by skipping symmetric paths.

9. **MED — Use bidirectional search for long paths.**
   Search from both start and end simultaneously; meet in the middle for ~50% reduction in explored nodes.

10. **MED — Limit BFS search radius more aggressively.**
    The `dist_sq > (max_path_length * max_path_length)` check compares distance, not path length. Use actual BFS depth.

11. **MED — Cache paths for entities that request the same start/end repeatedly.**

12. **MED — Use hierarchical pathfinding (HPA\*) for large maps.**
    Divide the map into regions, pathfind between regions first, then locally.

13. **MED — Replace `std::deque<Vec2>` paths with `std::vector<Vec2>`.**
    `deque` has per-chunk allocation overhead. Paths are typically built once and iterated, making `vector` more cache-friendly.

14. **MED — Use a condition variable instead of busy-wait polling in the pathfinding thread.**
    The thread currently spins with `sleep_for(1ms)` when idle. A condition variable wakes it immediately when work arrives.

15. **MED — Batch multiple path requests per thread wake-up.**
    Process all pending requests in one batch rather than waking per-request.

16. **MED — Replace `AtomicQueue` with a lock-free SPSC queue.**
    Single-producer single-consumer queues avoid mutex overhead entirely.

17. **MED — Use integer coordinates instead of float `Vec2` for grid pathfinding.**
    Integer comparison and hashing is faster than float.

18. **LOW — Pre-compute walkability into a bitset for O(1) lookup.**

19. **LOW — Use A\* with a Fibonacci heap for better theoretical complexity.**

20. **LOW — Smooth paths after computation (funnel algorithm) to reduce waypoint count.**

21. **LOW — Avoid `std::deque<Node*>` for BFS queue; use a `std::vector` with front pointer.**

22. **LOW — Reuse pathfinding data structures across requests (avoid reallocating maps/sets).**

23. **LOW — Use `reserve()` on the path result before reconstruction.**

24. **LOW — Inline the `reconstruct_path` lambda.**

25. **LOW — Use thread-local storage for pathfinding scratch buffers.**

26. **LOW — Profile and tune `MAX_ITERATIONS` (10000) — may be too high for simple maps.**

27. **LOW — Cache `is_walkable` results for recently queried cells.**

28. **LOW — Use `int16_t` for grid coordinates to halve memory usage in visited sets.**

29. **LOW — Avoid copying `std::function` in `PathRequest`; use `std::move`.**

30. **LOW — Use SIMD for distance calculations when processing multiple neighbors.**

31. **LOW — Pre-sort neighbors by heuristic distance to goal for better A\* node ordering.**

32. **LOW — Implement path request coalescing: if multiple entities want the same path, compute once.**

33. **LOW — Use memory-mapped walkability data for large maps.**

34. **LOW — Profile lock contention on `AtomicQueue` and consider reducing lock granularity.**

35. **LOW — Avoid `std::chrono::high_resolution_clock::now()` calls on every loop iteration; use a coarser timer.**
