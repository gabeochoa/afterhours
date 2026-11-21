// Unimplemented platform/backend paths log "@notimplemented ..." once per
// callsite instead of failing silently. Compiled with no backend defined, so
// every header below takes its none/stub arm. Follows the
// audio_stub_warn_test pattern: capture log_sink_fn, call twice, expect
// exactly one warning naming the function.
#define AFTER_HOURS_USE_NONE 1

#include <afterhours/src/plugins/clipboard.h>
#include <afterhours/src/drawing_helpers.h>
#include <afterhours/src/font_helper.h>
#include <afterhours/src/plugins/camera.h>
#include <afterhours/src/plugins/input_system.h>
#include <afterhours/src/plugins/models.h>
#include <afterhours/src/plugins/texture_manager.h>
#include <afterhours/src/plugins/window_manager.h>

#include <cstdio>
#include <string>

static int checks_run = 0;
static int checks_passed = 0;
static std::string captured;

static void check(bool cond, const char *what) {
  checks_run++;
  if (cond) {
    checks_passed++;
  } else {
    std::fprintf(stderr, "  FAIL: %s\n", what);
  }
}

static void capture_sink(const char *level, const char *message) {
  captured += level;
  captured += " ";
  captured += message;
  captured += "\n";
}

static int count_containing(const std::string &needle) {
  int n = 0;
  size_t pos = 0;
  while ((pos = captured.find(needle, pos)) != std::string::npos) {
    ++n;
    pos += needle.size();
  }
  return n;
}

// Call fn twice; expect exactly one @notimplemented line naming `name`.
template <typename F> static void check_once(F &&fn, const char *name) {
  const int before = count_containing(name);
  fn();
  fn();
  check(count_containing(name) == before + 1,
        (std::string("logs @notimplemented once: ") + name).c_str());
  check(count_containing("@notimplemented") >= 1, "message is tagged");
}

int main() {
  const auto previous = log_sink_fn;
  log_sink_fn = capture_sink;

  // clipboard (none fallback)
  check_once([] { afterhours::clipboard::set_text("x"); },
             "clipboard::set_text");
  check_once([] { (void)afterhours::clipboard::get_text(); },
             "clipboard::get_text");
  check_once([] { (void)afterhours::clipboard::has_text(); },
             "clipboard::has_text");
  check(afterhours::clipboard::get_text().empty(),
        "clipboard stub still returns empty");

  // none drawing helpers
  check_once(
      [] {
        afterhours::graphics::ShaderType s{};
        afterhours::begin_shader_mode(s);
      },
      "begin_shader_mode");
  check_once([] { afterhours::end_shader_mode(); }, "end_shader_mode");
  check_once(
      [] {
        const Vector2Type pts[4]{};
        afterhours::draw_quad(pts, afterhours::Color{});
      },
      "draw_quad");
  check_once([] { afterhours::set_mouse_cursor(0); }, "set_mouse_cursor");
  check_once([] { afterhours::draw_fps(0, 0); }, "draw_fps");
  check_once(
      [] {
        afterhours::graphics::RenderTextureType rt{};
        (void)afterhours::capture_render_texture_rgba(rt);
      },
      "capture_render_texture_rgba");
  check_once(
      [] {
        afterhours::graphics::RenderTextureType rt{};
        (void)afterhours::capture_render_texture_png(rt);
      },
      "capture_render_texture_png");

  // none font helper
  check_once([] { (void)afterhours::load_font_from_file("x.ttf"); },
             "load_font_from_file");
  check_once(
      [] {
        (void)afterhours::load_font_from_memory(nullptr, 0, "x");
      },
      "load_font_from_memory");
  check_once(
      [] { (void)afterhours::load_font_for_string("x", "y"); },
      "load_font_for_string");

  // models stub (non-raylib)
  check_once([] { (void)afterhours::load_model("x.glb"); }, "load_model");
  check_once(
      [] {
        afterhours::draw_model(afterhours::ModelType{},
                               Vector3Type{}, 0.f,
                               Vector3Type{});
      },
      "draw_model");
  check_once([] { (void)afterhours::load_lit_shader(); }, "load_lit_shader");
  check_once([] { (void)afterhours::load_instancing_shader(); },
             "load_instancing_shader");

  // texture_manager (no backend)
  check_once(
      [] {
        afterhours::texture_manager::draw_texture_pro(
            afterhours::texture_manager::Texture{},
            afterhours::texture_manager::Rectangle{},
            afterhours::texture_manager::Rectangle{},
            Vector2Type{}, 0.f,
            afterhours::texture_manager::Color{});
      },
      "texture_manager::draw_texture_pro");

  // camera stub
  check_once(
      [] {
        afterhours::camera::HasCamera cam;
        cam.set_position(1.f, 2.f);
      },
      "camera::HasCamera::set_position");
  check_once(
      [] {
        afterhours::camera::HasCamera cam;
        cam.set_zoom(2.f);
      },
      "camera::HasCamera::set_zoom");

  // window_manager (no backend)
  check_once([] { afterhours::window_manager::set_window_size(800, 600); },
             "window_manager::set_window_size");

  // input (no backend): the one poll that used to return silently
  check_once([] { (void)afterhours::input::is_key_pressed_repeat(65); },
             "is_key_pressed_repeat");
  check(!afterhours::input::is_key_pressed_repeat(65),
        "input stub still returns false");

  check_once(
      [] {
        afterhours::draw_line(0, 0, 10, 10, afterhours::Color{});
      },
      "draw_line");
  check_once([] { (void)afterhours::input::get_mouse_position(); },
             "get_mouse_position");
  check_once([] { (void)afterhours::input::is_key_pressed(65); },
             "is_key_pressed");
  check_once(
      [] {
        afterhours::input::draw_text("x", 0, 0, 12);
      },
      "draw_text");

  log_sink_fn = previous;
  std::printf("%d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
