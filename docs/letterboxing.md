# Resizing, scaling, pillarbox / letterbox

The library already supports this. Do not re-derive the math per game:
the window manager owns the content-to-window transform, and the input
system already applies it to the mouse.

## Model

Two coordinate spaces:

- **Content space** — the fixed size your game or UI lays out in, e.g.
  1280x720. Game logic, rendering into a render texture, and screenshots
  live here.
- **Window space** — the actual (resizable) window, in the same units as
  `graphics::get_screen_width()` / `get_screen_height()`.

`window_manager::ProvidesCurrentResolution` holds the content size.
`CollectCurrentResolution` (registered by
`window_manager::register_update_systems`) refreshes it from the window
every frame. On raylib that refresh fits the window to 16:9, so the
content size tracks the window's aspect; on sokol it is the logical
window size; without a backend it is 1280x720.

If you never register the singleton, the transform is identity: no
letterbox, and nothing breaks.

## Presenting your own render texture

Render into your texture, then present it with the dest rect from
`content_viewport`. It is the largest aspect-preserving rect that fits
the window, centred: side bars (pillarbox) when the window is wider
than the content aspect, top/bottom bars (letterbox) when it is
taller. `scale` is content pixels per window pixel.

```cpp
graphics::begin_drawing();
graphics::clear_background(raylib::BLACK); // clears the bars
draw_render_texture(target,
                    window_manager::content_viewport(
                        graphics::get_screen_width(),
                        graphics::get_screen_height())
                        .dest,
                    raylib::WHITE);
graphics::end_drawing();
```

The `dest` overload of `draw_render_texture` handles each backend's flip
(raylib stores render textures upside down; sokol does not), so the same
call works on both. The caller owns `begin/end_drawing` and clearing the
bars.

## Mapping points

- Window point to content point: `window_manager::window_to_content(p,
  window_w, window_h)`. A point on a bar is returned unchanged (it is
  outside the content, and clamping it would be a lie).
- Content point to window point: `window_manager::content_to_window(p,
  window_w, window_h)` — for OS cursor rects, IME windows, drag hit
  regions.
- Mouse input: `get_mouse_position()` already returns content space via
  `window_to_content`. Do not apply the mapping a second time.

## Fixed content that is not 16:9

The collector overwrites the singleton from the window (16:9 fit on
raylib), so a game with its own fixed size — e.g. 800x600 — must not
run it: register `CollectAvailableResolutions` on its own instead of
`register_update_systems`, and set the content size yourself with
`window_manager::add_singleton_components(entity, Resolution{w, h},
fps)`. `content_viewport` then letterboxes your size. For 16:9 content
this is unnecessary: the fitted size has the same aspect, so the dest
rect comes out identical either way.

## E2E screenshots

Capturing the render texture (`capture_render_texture_png`) bypasses
the present step, so screenshots are window-size independent and
contain no bars. That is the point of rendering into a texture.
