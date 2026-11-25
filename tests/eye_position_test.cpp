#include <cstdio>
#include <string>

namespace raylib {
#include <raylib.h>
// raymath zero-initialises with `= { 0 }`, which -Wextra flags once per use.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#include <raymath.h>
#pragma clang diagnostic pop
} // namespace raylib

#include <afterhours/ah.h>
#include <afterhours/src/graphics.h>
#include <afterhours/src/plugins/effects.h>
#include <afterhours/src/plugins/eye_position.h>
#include <afterhours/src/plugins/models.h>

using namespace afterhours;

static int checks_run = 0;
static int checks_passed = 0;

static void check(bool cond, const std::string &what) {
  checks_run++;
  if (cond) {
    checks_passed++;
  } else {
    fprintf(stderr, "  FAIL: %s\n", what.c_str());
  }
}

static bool is(Vector3Type v, float x, float y, float z) {
  return v.x == x && v.y == y && v.z == z;
}

int main() {
  raylib::SetTraceLogLevel(raylib::LOG_ERROR);
  afterhours::graphics::Config cfg;
  cfg.display = afterhours::graphics::DisplayMode::Headless;
  cfg.width = 64;
  cfg.height = 64;
  if (!afterhours::graphics::init(cfg)) {
    fprintf(stderr, "eye_position_test: headless graphics init failed\n");
    return 1;
  }

  check(is(eye_position::get(), 0.f, 0.f, 0.f), "default is the origin");

  eye_position::set({1.f, 2.f, 3.f});
  check(is(eye_position::get(), 1.f, 2.f, 3.f), "set/get round-trips");

  {
    eye_position::Scope scope({4.f, 5.f, 6.f});
    check(is(eye_position::get(), 4.f, 5.f, 6.f), "scope sets");
  }
  check(is(eye_position::get(), 1.f, 2.f, 3.f), "scope restores");

  eye_position::reset();
  check(is(eye_position::get(), 0.f, 0.f, 0.f), "reset returns to origin");

  ShaderType shader = load_lit_shader();
  check(raylib::GetShaderLocation(shader, "camPos") != -1,
        "lit shader declares camPos");
  eye_position::set({7.f, 8.f, 9.f});
  apply_lighting(shader, Lighting{});
  apply_lighting(shader, Lighting{}, Vector3Type{7.f, 8.f, 9.f});
  raylib::UnloadShader(shader);

  effects::Effect effect;
  effect.set("camPos", eye_position::get());
  check(!effect.loaded, "unloaded effect ignores the eye position set");

  eye_position::reset();
  afterhours::graphics::shutdown();

  printf("eye_position_test: %d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
