# Camera Plugin

1. **MED — Cache the `HasCamera` singleton pointer instead of querying every frame.**
   `BeginCameraMode::once()` and `EndCameraMode::once()` both call `get_singleton_cmp<HasCamera>()` every frame.

2. **MED — Avoid `BeginMode2D` / `EndMode2D` when the camera hasn't changed.**
   If position, zoom, and rotation are unchanged, skip the state change.

3. **MED — Combine `BeginCameraMode` and `EndCameraMode` into a single RAII system.**
   Reduces the number of systems in the render pipeline.

4. **LOW — Use integer coordinates for pixel-perfect rendering to avoid sub-pixel jitter.**

5. **LOW — Pre-compute the camera matrix once per frame instead of letting raylib recalculate.**

6. **LOW — Cache the view-projection matrix for frustum culling in other systems.**

7. **LOW — Avoid redundant `if (camera_entity)` null checks when singleton is guaranteed.**

8. **LOW — Use `constexpr` for default camera values.**

9. **LOW — Profile `BeginMode2D` overhead and consider manual matrix setup.**

10. **LOW — Implement camera interpolation (lerp) to reduce the number of position updates.**

11. **LOW — Use dirty flag for camera to skip unnecessary `set_position` calls.**

12. **LOW — Pre-compute screen-to-world and world-to-screen transforms for click handling.**

13. **LOW — Avoid floating-point rounding in camera offset calculations.**

14. **LOW — Cache camera bounds (visible rect) for culling optimizations.**

15. **LOW — Use SIMD for camera matrix multiplication.**

16. **LOW — Combine camera transform with sprite rendering to reduce per-sprite matrix operations.**

17. **LOW — Profile and optimize camera shake effects (if used) to avoid per-frame trig calls.**

18. **LOW — Use a camera stack for nested camera contexts (e.g., minimap) to avoid redundant setup.**

19. **LOW — Avoid `EnforceSingleton<HasCamera>` system running every frame; enforce once at init.**

20. **LOW — Pre-compute zoom-dependent constants (tile visibility, LOD levels).**

21. **LOW — Use integer zoom levels for pixel art to avoid texture filtering artifacts.**

22. **LOW — Cache the camera entity reference to avoid singleton map lookup.**

23. **LOW — Profile raylib's `BeginMode2D` implementation for optimization opportunities.**

24. **LOW — Avoid redundant `set_zoom` / `set_rotation` calls when values haven't changed.**

25. **LOW — Use platform-specific optimizations (Metal, Vulkan) for camera matrix setup.**

26. **LOW — Batch camera-dependent calculations (world-to-screen) for all entities at once.**

27. **LOW — Consider using a projection matrix cache that invalidates only on resolution change.**

28. **LOW — Avoid virtual dispatch for camera systems; they're always concrete types.**

29. **LOW — Profile the overhead of two separate systems (begin/end) vs. a single wrapper.**

30. **LOW — Use `alignas` on camera data structures for better cache performance.**

31. **LOW — Consider removing the non-raylib stub path entirely if not used in production.**

32. **LOW — Pre-compute `camera.offset` from window dimensions once per resolution change.**

33. **LOW — Use a ring buffer for camera position history (for smoothing/replay).**

34. **LOW — Inline camera accessor methods (`set_position`, `set_zoom`, etc.).**

35. **LOW — Profile memory access patterns when camera data is accessed alongside transforms.**
