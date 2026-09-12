#include "ui_test_harness.h"

#include <afterhours/src/plugins/input_system.h>

#include <cstdio>
#include <string>

using namespace afterhours;
using MouseAxis = input::MouseAxis;
using MouseAxisWithDir = input::MouseAxisWithDir;

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
  printf("=== mouse axis as an action ===\n\n");

  // A mouse axis is a member of the same variant a key or a stick is, which is
  // the whole point: camera look binds like everything else.
  {
    input::AnyInput as_input = MouseAxisWithDir{MouseAxis::X, 1};
    check(as_input.index() == 3, "a mouse axis is a bindable input");
  }

  // The scale is what makes a mouse comparable to a stick. Without it the two
  // report different units through the same float.
  {
    const input::ProvidesInputConfig cfg;
    printf("  default scale: %.0f px = full strength\n", cfg.mouse_delta_scale);
    check(cfg.mouse_delta_scale > 0.f,
          "there is a default scale, so an unconfigured app still works");
  }

  // Direction is part of the binding: look-left and look-right are separate
  // actions off the same axis, exactly like a gamepad stick.
  {
    const MouseAxisWithDir left{MouseAxis::X, -1};
    const MouseAxisWithDir right{MouseAxis::X, 1};
    check(left.dir != right.dir, "opposite directions are distinct bindings");
    check(left.axis == right.axis, "off the same axis");
  }

  // X and Y are distinct, so look-horizontal and look-vertical do not collide.
  {
    const MouseAxisWithDir x{MouseAxis::X, 1};
    const MouseAxisWithDir y{MouseAxis::Y, 1};
    check(x.axis != y.axis, "the two axes are separate");
  }

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
