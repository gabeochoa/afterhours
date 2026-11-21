// eye_position_test.cpp
// The current eye position is plain state: default, set/get round-trip,
// reset, and a scope that restores what was there. The wired paths
// (apply_lighting's no-camera overload, Effect::set with a Vector3) run
// against a real shader in a hidden window.

#include <cstdio>
#include <string>

#include <afterhours/ah.h>
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
  raylib::SetConfigFlags(raylib::FLAG_WINDOW_HIDDEN);
  raylib::InitWindow(64, 64, "eye_position_test");

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

  // No-camera apply_lighting reads the eye position for camPos.
  ShaderType shader = load_lit_shader();
  check(raylib::GetShaderLocation(shader, "camPos") != -1,
        "lit shader declares camPos");
  eye_position::set({7.f, 8.f, 9.f});
  apply_lighting(shader, Lighting{});
  apply_lighting(shader, Lighting{}, Vector3Type{7.f, 8.f, 9.f});
  raylib::UnloadShader(shader);

  // Effect::set with a Vector3 is a no-op until a shader is loaded; the
  // overload exists so an effect can take the eye position directly.
  effects::Effect effect;
  effect.set("camPos", eye_position::get());
  check(!effect.loaded, "unloaded effect ignores the eye position set");

  eye_position::reset();
  raylib::CloseWindow();

  printf("eye_position_test: %d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
