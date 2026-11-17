#if !defined(AFTER_HOURS_USE_RAYLIB) && !defined(AFTER_HOURS_USE_VULKAN) && !defined(AFTER_HOURS_USE_SOKOL)
#define AFTER_HOURS_USE_SOKOL
#endif

#include <afterhours/src/graphics.h>
#include <afterhours/src/backends/sokol/drawing_helpers.h>

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

namespace g = afterhours::graphics;
namespace md = afterhours::graphics::metal_detail;

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

static std::vector<float> dts;

static void callback_after(int ms) {
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
  md::sokol_frame_cb();
}

int main() {
  printf("=== sokol frame dt ===\n\n");
  stm_setup();
  md::g_frame_fn = [] { dts.push_back(g::get_frame_time()); };

  md::g_target_fps = 0;
  md::g_on_demand = false;
  md::sokol_frame_cb();
  callback_after(50);
  callback_after(20);
  printf("  continuous: %.3f %.3f\n", dts[1], dts[2]);
  check(dts.size() == 3, "every callback ran a frame");
  check(dts[1] >= 0.045f && dts[1] < 0.5f, "dt is the 50ms since the last frame");
  check(dts[2] >= 0.015f && dts[2] < dts[1], "and then the 20ms after that");

  md::g_on_demand = true;
  for (int i = 0; i < 4; ++i)
    callback_after(10);
  check(dts.size() == 3, "idle callbacks run no frame");
  afterhours::frame_loop::request_frame();
  callback_after(10);
  printf("  after 4 skipped callbacks: %.3f\n", dts.back());
  check(dts.size() == 4 && dts.back() >= 0.045f,
        "dt spans the skipped callbacks, so timers keep wall time");

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
