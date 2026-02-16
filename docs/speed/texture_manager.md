# Texture Manager Plugin

1. **MED — Batch sprite draw calls with the same texture.**
   `RenderSprites` and `RenderAnimation` draw one sprite at a time. Batch all sprites sharing the same spritesheet into a single draw call.

2. **MED — Cache the spritesheet texture in the system instead of querying singleton every frame.**
   `once()` queries the singleton every frame. Cache it once at system creation.

3. **MED — Use texture atlases to reduce draw call count.**

4. **MED — Pre-compute `destination()` rectangle instead of computing per-frame for static sprites.**

5. **MED — Use instanced rendering for sprites with the same frame.**

6. **MED — Sort sprites by texture before rendering to minimize state changes.**

7. **MED — Pre-compute `idx_to_sprite_frame` results into a lookup table.**
   Already `constexpr`, but ensure it's actually evaluated at compile time.

8. **LOW — Use `float` directly instead of `(float)int` casts in frame calculation.**

9. **LOW — Avoid `std::pair<int,int>` return in `idx_to_next_sprite_location`; use a struct.**

10. **LOW — Skip rendering off-screen sprites (frustum culling).**

11. **LOW — Use GPU instancing for large numbers of identical sprites.**

12. **LOW — Pool-allocate `HasSprite` / `HasAnimation` components.**

13. **LOW — Avoid calling `center()` in `destination()`; cache the center position.**

14. **LOW — Use `constexpr` for AFTERHOURS_SPRITE_SIZE_PX calculations.**

15. **LOW — Pre-compute animation frame sequences at load time.**

16. **LOW — Reduce `HasAnimation::Params` struct size by packing fields.**

17. **LOW — Use `uint16_t` for frame indices to reduce memory.**

18. **LOW — Avoid `Vector2Type` copies in `TransformData::center()`; return by value is fine but ensure NRVO.**

19. **LOW — Profile and tune sprite batch sizes for the target GPU.**

20. **LOW — Use a sprite atlas packer to minimize texture waste.**

21. **LOW — Avoid redundant `entity.cleanup = true` for finished one-shot animations; batch cleanup.**

22. **LOW — Pre-compute `frame.width * scale` and `frame.height * scale` once per animation.**

23. **LOW — Use mipmapping for sprites that are frequently scaled down.**

24. **LOW — Avoid `draw_texture_pro` call overhead; batch into a vertex buffer.**

25. **LOW — Profile texture sampling and consider nearest-neighbor for pixel art.**

26. **LOW — Use compressed textures (DXT/ETC) to reduce GPU memory bandwidth.**

27. **LOW — Skip alpha-blending for fully opaque sprites.**

28. **LOW — Pre-load textures in a background thread.**

29. **LOW — Use texture streaming for large open-world maps.**

30. **LOW — Profile and optimize the `HasAnimation` update loop (frame timing).**

31. **LOW — Avoid floating-point animation frame position; use integer frame index.**

32. **LOW — Cache `HasSpritesheet` texture reference to avoid singleton lookup per frame.**

33. **LOW — Use `alignas` on sprite data for better cache line utilization.**

34. **LOW — Consider sprite batching libraries (e.g., raylib's built-in batch mode).**

35. **LOW — Reduce draw call state changes by sorting: texture -> blend mode -> shader.**
