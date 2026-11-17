#include <afterhours/src/graphics.h>
#include <afterhours/src/plugins/e2e_testing/test_input.h>

#include <cstdio>
#include <string>

namespace gfx = afterhours::graphics;
namespace inj = afterhours::testing::input_injector;
namespace keys = afterhours::keys;

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

int main() {
  printf("=== graphics key reads see injected keys ===\n\n");

  inj::reset_all();
  afterhours::testing::test_input::push_key(keys::S);
  check(!gfx::is_key_pressed(keys::S), "press not visible on the inject frame");
  inj::reset_frame();
  check(gfx::is_key_pressed(keys::S), "is_key_pressed sees the push_key");
  check(gfx::is_key_pressed(keys::S), "and a second reader in the frame");
  check(gfx::is_key_pressed_repeat(keys::S), "is_key_pressed_repeat too");
  check(!gfx::is_key_released(keys::S), "no release on the press frame");
  inj::reset_frame();
  check(!gfx::is_key_pressed(keys::S), "press spent after its frame");
  check(gfx::is_key_released(keys::S), "release lands the frame after");
  inj::reset_frame();
  check(!gfx::is_key_released(keys::S), "release spent after its frame");

  inj::reset_all();
  inj::set_key_held(keys::LEFT_SUPER);
  check(gfx::is_key_down(keys::LEFT_SUPER), "is_key_down sees a held key");
  check(!gfx::is_key_pressed(keys::LEFT_SUPER), "held is not a press");
  inj::set_key_up(keys::LEFT_SUPER);
  check(!gfx::is_key_down(keys::LEFT_SUPER), "released key is up");
  check(!gfx::is_key_released(keys::LEFT_SUPER), "release waits a frame");
  inj::reset_frame();
  check(gfx::is_key_released(keys::LEFT_SUPER), "then is_key_released fires");

  inj::reset_all();
  inj::set_key_up(keys::A);
  inj::reset_frame();
  check(!gfx::is_key_released(keys::A), "releasing an up key is no edge");

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
