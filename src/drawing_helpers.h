
#pragma once

#include "config.h"

// Rect geometry lives beside the draw calls that consume it.
#include "rect.h"

#ifdef AFTER_HOURS_USE_RAYLIB
#include "backends/raylib/drawing_helpers.h"
#elif defined(AFTER_HOURS_SOKOL)
#include "backends/sokol/drawing_helpers.h"
#else
// The `none` backend records draw calls rather than issuing them (see its
// drawing_helpers.h). Callers that want to assert on what was drawn -- tests,
// mostly -- can key off this instead of re-deriving the backend condition.
#define AFTER_HOURS_BACKEND_NONE
#include "backends/none/drawing_helpers.h"
#endif

namespace afterhours {

// Text layout for raw (non-UI) drawing. Anchors: centered takes the text
// centre, right-aligned takes the text's right edge, with_shadow takes the
// text's top-left and draws the shadow first. For centred + shadow,
// measure with measure_text and offset the with_shadow call by half the
// width. Spacing matches draw_text_ex.
inline void draw_text_centered(const Font font, const char *text,
                               const float center_x, const float y,
                               const float size, const Color color,
                               const float spacing = 1.f) {
  if (!text)
    return;
  const float w = measure_text(font, text, size, spacing).x;
  draw_text_ex(font, text, Vector2Type{center_x - w * 0.5f, y}, size, spacing,
               color);
}

inline void draw_text_right_aligned(const Font font, const char *text,
                                    const float right_x, const float y,
                                    const float size, const Color color,
                                    const float spacing = 1.f) {
  if (!text)
    return;
  const float w = measure_text(font, text, size, spacing).x;
  draw_text_ex(font, text, Vector2Type{right_x - w, y}, size, spacing, color);
}

inline void draw_text_with_shadow(const Font font, const char *text,
                                  const float x, const float y,
                                  const float size, const Color color,
                                  const Color shadow_color,
                                  const float shadow_offset_x = 2.f,
                                  const float shadow_offset_y = 2.f,
                                  const float spacing = 1.f) {
  if (!text)
    return;
  draw_text_ex(font, text,
               Vector2Type{x + shadow_offset_x, y + shadow_offset_y}, size,
               spacing, shadow_color);
  draw_text_ex(font, text, Vector2Type{x, y}, size, spacing, color);
}

} // namespace afterhours

// graphics::run() with canvas_width/height set: draw into the canvas, then
// present it letterboxed. Here because it needs the texture helpers above.
#if defined(AFTER_HOURS_USE_RAYLIB) || defined(AFTER_HOURS_SOKOL) ||           \
    defined(AFTER_HOURS_USE_NONE)
#include <algorithm>
#include <cmath>
#include <memory>

#include "graphics.h"

namespace afterhours::graphics::detail {
inline bool run_with_canvas(RunConfig cfg) {
  const int w = cfg.canvas_width, h = cfg.canvas_height;
  auto rt = std::make_shared<RenderTextureType>();
  cfg.init = [rt, w, h, init = cfg.init] {
    *rt = load_render_texture(w, h);
    canvas() = rt.get();
    if (init)
      init();
  };
  cfg.frame = [rt, w, h, frame = cfg.frame] {
    begin_texture_mode(*rt);
    if (frame)
      frame();
    end_texture_mode();
    begin_drawing();
    clear_background(ColorType{0, 0, 0, 255}); // the letterbox bars
    const float sw = static_cast<float>(get_screen_width());
    const float sh = static_cast<float>(get_screen_height());
    const float s = std::min(sw / static_cast<float>(w),
                             sh / static_cast<float>(h));
    const RectangleType dest = rect::align_center(
        RectangleType{0, 0, sw, sh}, std::round(static_cast<float>(w) * s),
        std::round(static_cast<float>(h) * s));
    draw_render_texture(*rt, dest, ColorType{255, 255, 255, 255});
    end_drawing();
  };
  cfg.cleanup = [rt, cleanup = cfg.cleanup] {
    if (cleanup)
      cleanup();
    canvas() = nullptr;
    unload_render_texture(*rt);
  };
  return PlatformAPI::run(cfg);
}
} // namespace afterhours::graphics::detail
#endif
