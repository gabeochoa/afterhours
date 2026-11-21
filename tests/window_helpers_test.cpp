// window_helpers_test.cpp
// Fit/centre geometry is pure math, so it is checked headlessly here. Real
// window placement (raylib SetWindowPosition / AppKit) needs a human looking
// at a real window and is not covered by this test.

#define AFTER_HOURS_USE_RAYLIB

#include <afterhours/src/graphics_common.h>
#include <afterhours/src/window_helpers.h>

#include <cstdio>
#include <string>
#include <utility>

using afterhours::graphics::centre_window_on_display;
using afterhours::graphics::Config;
using afterhours::graphics::fit_window_size;
using afterhours::graphics::window_centre_position;

static int checks_run = 0;
static int checks_passed = 0;

static void check(bool cond, const std::string &what) {
  checks_run++;
  if (cond) {
    checks_passed++;
  } else {
    fprintf(stderr, "FAIL: %s\n", what.c_str());
  }
}

int main() {
  check(fit_window_size(2000, 900, 1920, 1080) == std::pair{1920, 900},
        "fit clamps each axis to the display");
  check(fit_window_size(1280, 720, 1920, 1080) == std::pair{1280, 720},
        "fit leaves a window that already fits untouched");
  check(fit_window_size(320, 180, 1920, 1080, 800, 600) == std::pair{800, 600},
        "fit raises to the min size");
  check(fit_window_size(320, 180, 640, 480, 800, 600) == std::pair{800, 600},
        "min larger than the display wins");
  check(fit_window_size(1280, 720, 1920, 1080, 800, 600) ==
            std::pair{1280, 720},
        "fit above the min is unchanged");

  check(window_centre_position(0, 0, 1920, 1080, 1280, 720) ==
            std::pair{320, 180},
        "centre on the primary display");
  check(window_centre_position(1920, 0, 1920, 1080, 1280, 720) ==
            std::pair{2240, 180},
        "centre on an offset (second) display");
  check(window_centre_position(0, 0, 1920, 1080, 2000, 1200) == std::pair{0, 0},
        "oversized window pins to the display origin");
  check(window_centre_position(-1920, 0, 1920, 1080, 1280, 720) ==
            std::pair{-1600, 180},
        "centre on a display left of the primary");

  const auto fitted =
      centre_window_on_display(0, 0, 1920, 1080, 2200, 900, 800, 600);
  check(fitted.width == 1920 && fitted.height == 900 && fitted.x == 0 &&
            fitted.y == 90,
        "centre_window_on_display fits then centres");

  Config cfg;
  cfg.width = 2560;
  cfg.height = 1440;
  const auto cfg_fitted =
      centre_window_on_display(0, 0, 1920, 1080, cfg.width, cfg.height);
  check(cfg_fitted.width == 1920 && cfg_fitted.height == 1080 &&
            cfg_fitted.x == 0 && cfg_fitted.y == 0,
        "Config width/height plumb through the fit");

  printf("%d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
