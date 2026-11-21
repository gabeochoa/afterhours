#pragma once
#include "drawing_helpers.h"
#include "graphics.h"
namespace afterhours {
struct TextureModeScope {
  explicit TextureModeScope(graphics::RenderTextureType &rt) {
    begin_texture_mode(rt);
  }
  ~TextureModeScope() { end_texture_mode(); }
  TextureModeScope(const TextureModeScope &) = delete;
  TextureModeScope &operator=(const TextureModeScope &) = delete;
};
struct OffscreenLayer {
  graphics::RenderTextureType rt{};
  bool loaded = false;
  blend::Mode blend_before_ = blend::Mode::Alpha;
  OffscreenLayer() = default;
  OffscreenLayer(int w, int h) { resize(w, h); }
  ~OffscreenLayer() { unload(); }
  OffscreenLayer(const OffscreenLayer &) = delete;
  OffscreenLayer &operator=(const OffscreenLayer &) = delete;
  void resize(int w, int h) {
    unload();
    rt = load_render_texture(w, h);
    loaded = true;
  }
  void unload() {
    if (loaded)
      unload_render_texture(rt);
    loaded = false;
  }
  void begin(Color clear = Color{0, 0, 0, 0}) {
    blend_before_ = blend::current();
    begin_texture_mode(rt);
    graphics::clear_background(clear);
    set_blend_mode(blend::Mode::Layer);
  }
  void end() {
    set_blend_mode(blend_before_);
    end_texture_mode();
  }
  void composite(float x, float y, Color tint = Color{255, 255, 255, 255}) {
    blend_scope premultiplied(blend::Mode::AlphaPremultiply);
    draw_render_texture(rt, x, y, tint);
  }
};
}
