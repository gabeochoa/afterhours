// NonePlatformAPI: run() drives init/frame/cleanup with no window or GPU, and
// injected e2e input reaches the input plugin, so headless builds play scripts.
#define AFTER_HOURS_ENABLE_E2E_TESTING
#include <afterhours/src/graphics.h>
#include <afterhours/src/drawing_helpers.h>
#include <afterhours/src/plugins/input_system.h>
#include <afterhours/src/plugins/e2e_testing/input_injector.h>

#include <cstdio>
#include <string>

using namespace afterhours;

static int checks_run = 0;
static int checks_passed = 0;

static void check(bool cond, const std::string &what) {
  checks_run++;
  if (cond)
    checks_passed++;
  else
    fprintf(stderr, "  FAIL: %s\n", what.c_str());
}

int main() {
  printf("=== none platform ===\n\n");
  int frames = 0;
  bool inited = false, cleaned = false;
  float fps = 0.f, frame_time = 0.f;
  double time = 0.0;
  int w = 0, h = 0;

  graphics::RunConfig cfg;
  cfg.width = 64;
  cfg.height = 32;
  cfg.init = [&] { inited = true; };
  cfg.frame = [&] {
    graphics::RenderTextureType rt = load_render_texture(64, 32);
    begin_texture_mode(rt);
    end_texture_mode();
    graphics::begin_drawing();
    draw_render_texture(rt, RectangleType{0, 0, 64, 32}, ColorType{});
    graphics::end_drawing();
    unload_render_texture(rt);
    if (++frames == 10) {
      fps = graphics::get_fps();
      frame_time = graphics::get_frame_time();
      time = graphics::get_time();
      w = graphics::get_screen_width();
      h = graphics::get_screen_height();
      graphics::request_quit();
    }
  };
  cfg.cleanup = [&] { cleaned = true; };

  check(graphics::run(cfg), "run() returns true");
  check(inited && cleaned && frames == 10, "init, 10 frames, cleanup");
  check(w == 64 && h == 32, "screen size is the configured size");
  check(fps > 0.f && frame_time > 0.f && time > 0.0, "the clock ticks");
  check(!graphics::is_window_ready(), "the window closes after run()");

  testing::input_injector::set_key_down(keys::W);
  check(input::is_key_down(keys::W), "injected keys reach input::is_key_down");
  testing::input_injector::set_key_up(keys::W);
  check(!input::is_key_down(keys::W), "and release");

  const input::ValidInputs bindings = {keys::W, input::gamepad::LEFT_FACE_UP};
  check(std::holds_alternative<input::GamepadButton>(bindings[1]),
        "a gamepad constant binds as a button, not a key code");

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
