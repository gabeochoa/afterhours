#pragma once

// Present the engine render texture into the window, letterboxed to the
// texture's aspect. Presentation uses the same logical content viewport as
// input mapping (`window_manager::content_viewport`), so the pixels land
// where `get_mouse_position` says they are. The render texture may be
// supersampled; its aspect ratio is unchanged.
//
// Render textures are stored bottom-up, so the source rect carries the
// negative height flip that present already needs. On backends with no
// render texture (none/psx) presenting is a no-op.

#include "../drawing_helpers.h"
#include "../graphics.h"
#include "color.h"
#include "window_manager.h"

namespace afterhours {

// Where the content lands inside the window, in window space.
inline RectangleType letterbox_dest() {
  return window_manager::content_viewport(graphics::get_screen_width(),
                                          graphics::get_screen_height())
      .dest;
}

// Blit the engine render texture into `dest`, optionally under `shader`.
inline void present_render_texture(RectangleType dest,
                                   graphics::ShaderType *shader = nullptr) {
#if defined(AFTER_HOURS_USE_RAYLIB)
  graphics::RenderTextureType &rt = graphics::get_render_texture();
  const TextureType tex = rt.texture;
  const float w = static_cast<float>(tex.width);
  const float h = static_cast<float>(tex.height);
#elif defined(AFTER_HOURS_SOKOL)
  graphics::RenderTextureType &rt = graphics::get_render_texture();
  const TextureType tex{static_cast<float>(rt.width),
                        static_cast<float>(rt.height), rt.color_img_id,
                        rt.tex_view_id, rt.sampler_id};
  const float w = static_cast<float>(rt.width);
  const float h = static_cast<float>(rt.height);
#else
  (void)dest;
  (void)shader;
  return;
#endif

  const RectangleType src{0.f, 0.f, w, -h};
  if (shader != nullptr) graphics::begin_shader_mode(*shader);
  draw_texture_pro(tex, src, dest, Vector2Type{0.f, 0.f}, 0.f, colors::WHITE);
  if (shader != nullptr) graphics::end_shader_mode();
}

// Blit into the letterboxed dest for the current window.
inline void present_render_texture(graphics::ShaderType *shader = nullptr) {
  present_render_texture(letterbox_dest(), shader);
}

} // namespace afterhours
