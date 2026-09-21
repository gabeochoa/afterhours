#pragma once

#include <string>
#include <unordered_map>
#include <vector>

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
