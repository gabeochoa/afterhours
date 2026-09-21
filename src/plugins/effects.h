#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "../drawing_helpers.h"
#include "../graphics.h"
#include "files.h"
#include "terminal/commands.h"

namespace afterhours {
namespace effects {

struct Effect {
  std::string name;
  std::string path;
  graphics::ShaderType shader{};
  bool loaded = false;
  std::unordered_map<std::string, int> locations;

  static Effect load(const std::string &name, const std::string &file = "") {
    Effect e;
    e.name = name;
    e.path = files::get_resource_path("shaders", file.empty() ? name + ".fs" : file).string();
    e.reload();
    registry().push_back(&e);
    return e;
  }

  ~Effect() {
    auto &r = registry();
    std::erase(r, this);
    if (loaded)
      graphics::unload_shader(shader);
  }
  Effect() = default;
  Effect(const Effect &) = delete;
  Effect &operator=(const Effect &) = delete;
  Effect(Effect &&other) noexcept { *this = std::move(other); }
  Effect &operator=(Effect &&other) noexcept {
    if (this == &other)
      return *this;
    auto &r = registry();
    std::erase(r, &other);
    std::erase(r, this);
    name = std::move(other.name);
    path = std::move(other.path);
    shader = other.shader;
    loaded = other.loaded;
    locations = std::move(other.locations);
    other.loaded = false;
    r.push_back(this);
    return *this;
  }

  void reload() {
    if (loaded)
      graphics::unload_shader(shader);
    locations.clear();
    shader = graphics::load_shader(nullptr, path.c_str());
    loaded = shader.id != 0;
    if (!loaded)
      log_error("effects: could not load shader '{}' from {}", name, path);
  }

  bool ok() const { return loaded; }

  int location(const char *uniform) {
    auto it = locations.find(uniform);
    if (it != locations.end())
      return it->second;
    const int loc = graphics::get_shader_location(shader, uniform);
    locations.emplace(uniform, loc);
    return loc;
  }

  void set(const char *uniform, float v) {
    if (loaded)
      graphics::set_shader_value(shader, location(uniform), &v, graphics::SHADER_UNIFORM_FLOAT);
  }
  void set(const char *uniform, Vector2Type v) {
    const float arr[2] = {v.x, v.y};
    if (loaded)
      graphics::set_shader_value(shader, location(uniform), arr, graphics::SHADER_UNIFORM_VEC2);
  }
  void set(const char *uniform, ColorType c) {
    const float arr[4] = {c.r / 255.f, c.g / 255.f, c.b / 255.f, c.a / 255.f};
    if (loaded)
      graphics::set_shader_value(shader, location(uniform), arr, graphics::SHADER_UNIFORM_VEC4);
  }

  struct Scope {
    bool active = false;
    explicit Scope(Effect &e) : active(e.loaded) {
      if (active)
        graphics::begin_shader_mode(e.shader);
    }
    ~Scope() {
      if (active)
        graphics::end_shader_mode();
    }
  };

  static std::vector<Effect *> &registry() {
    static std::vector<Effect *> effects;
    return effects;
  }
};

struct BlurPass {
  Effect effect;
  graphics::RenderTextureType a{};
  graphics::RenderTextureType b{};
  int w = 0, h = 0;
  bool ready = false;

  BlurPass() : effect(Effect::load("blur")) {}
  ~BlurPass() {
    if (ready) {
      unload_render_texture(a);
      unload_render_texture(b);
    }
  }

  void ensure(int width, int height) {
    if (ready && width == w && height == h)
      return;
    if (ready) {
      unload_render_texture(a);
      unload_render_texture(b);
    }
    w = std::max(1, width);
    h = std::max(1, height);
    a = load_render_texture(w, h);
    b = load_render_texture(w, h);
    graphics::set_render_texture_filter(a, graphics::TEXTURE_FILTER_BILINEAR);
    graphics::set_render_texture_filter(b, graphics::TEXTURE_FILTER_BILINEAR);
    ready = true;
  }

  static bool usable(const Effect &effect, float radius, RectangleType rect) {
    return effect.ok() && radius >= 0.25f && rect.width >= 2.f && rect.height >= 2.f;
  }

  static RectangleType clamp_to(RectangleType rect, float fw, float fh) {
    rect.x = std::clamp(rect.x, 0.f, fw);
    rect.y = std::clamp(rect.y, 0.f, fh);
    rect.width = std::min(rect.width, fw - rect.x);
    rect.height = std::min(rect.height, fh - rect.y);
    return rect;
  }

  void blur_a(float radius) {
    const RectangleType half{0.f, 0.f, static_cast<float>(w), static_cast<float>(h)};
    const ColorType white{255, 255, 255, 255};
    begin_texture_mode(b);
    effect.set("direction", Vector2Type{radius / 6.46f / static_cast<float>(w), 0.f});
    {
      Effect::Scope scope(effect);
      draw_texture_pro(a.texture, {0.f, 0.f, half.width, -half.height}, half, {0.f, 0.f}, 0.f, white);
    }
    end_texture_mode();

    begin_texture_mode(a);
    effect.set("direction", Vector2Type{0.f, radius / 6.46f / static_cast<float>(h)});
    {
      Effect::Scope scope(effect);
      draw_texture_pro(b.texture, {0.f, 0.f, half.width, -half.height}, half, {0.f, 0.f}, 0.f, white);
    }
    end_texture_mode();
  }

  void apply(graphics::RenderTextureType &frame, RectangleType rect, float radius) {
    radius = std::clamp(radius, 0.f, 8.f);
    if (!usable(effect, radius, rect))
      return;
    const float fw = static_cast<float>(frame.texture.width);
    const float fh = static_cast<float>(frame.texture.height);
    rect = clamp_to(rect, fw, fh);
    end_texture_mode();
    ensure(static_cast<int>(rect.width / 2.f), static_cast<int>(rect.height / 2.f));
    const RectangleType half{0.f, 0.f, static_cast<float>(w), static_cast<float>(h)};
    const RectangleType src{rect.x, fh - rect.y - rect.height, rect.width, -rect.height};
    const ColorType white{255, 255, 255, 255};

    begin_texture_mode(a);
    draw_texture_pro(frame.texture, src, half, {0.f, 0.f}, 0.f, white);
    end_texture_mode();

    blur_a(radius);

    begin_texture_mode(frame);
    draw_texture_pro(a.texture, {0.f, 0.f, half.width, -half.height}, rect, {0.f, 0.f}, 0.f, white);
  }

  void apply_screen(RectangleType rect, float radius, float pixel_scale, Vector2Type screen_px) {
    radius = std::clamp(radius, 0.f, 8.f);
    if (!usable(effect, radius, rect) || pixel_scale <= 0.f)
      return;
    rect = clamp_to(rect, screen_px.x / pixel_scale, screen_px.y / pixel_scale);
    const RectangleType src{rect.x * pixel_scale, screen_px.y - (rect.y + rect.height) * pixel_scale,
                            rect.width * pixel_scale, rect.height * pixel_scale};
    ensure(static_cast<int>(src.width / 2.f), static_cast<int>(src.height / 2.f));
    copy_screen_to_render_texture(a, src);
    blur_a(radius);
    const RectangleType half{0.f, 0.f, static_cast<float>(w), static_cast<float>(h)};
    draw_texture_pro(a.texture, {0.f, 0.f, half.width, -half.height}, rect, {0.f, 0.f}, 0.f, {255, 255, 255, 255});
  }
};

inline int reload_all() {
  int count = 0;
  for (Effect *e : Effect::registry()) {
    e->reload();
    count += e->ok() ? 1 : 0;
  }
  return count;
}

inline terminal::Command reload_command() {
  return terminal::Command{
      .name = "shaders",
      .help = "shaders reload: reload every effect shader from disk",
      .run = [](terminal::Arguments args) -> terminal::Result {
        if (args.empty() || args.front() != "reload")
          return terminal::Result{.text = "usage: shaders reload", .success = false};
        const int ok = reload_all();
        return terminal::Result{.text = std::to_string(ok) + " shader(s) reloaded"};
      },
      .completions = {"reload"},
  };
}

} // namespace effects
} // namespace afterhours
