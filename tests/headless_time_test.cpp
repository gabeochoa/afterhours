// headless_time_test.cpp
// Headless get_time() must be simulated, not wall-clock: it advances by
// get_delta_time() per frame and resets on init, regardless of sleeps.

#include <afterhours/src/graphics.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <thread>

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

static bool near(double a, double b) { return std::abs(a - b) < 1e-6; }

static void step_frames(int n) {
  for (int i = 0; i < n; i++) {
    graphics::begin_frame();
    graphics::end_frame();
  }
}

static double expected_time(int frames, float time_scale) {
  double t = 0.0;
  const float dt = (1.0f / 60.0f) * time_scale;
  for (int i = 0; i < frames; i++)
    t += static_cast<double>(dt);
  return t;
}

int main() {
  graphics::Config cfg;
  cfg.display = graphics::DisplayMode::Headless;
  cfg.width = 64;
  cfg.height = 64;
  cfg.target_fps = 60;
  cfg.time_scale = 1.0f;
  graphics::raylib_backend::ensure_registered();
  check(graphics::init(cfg), "headless init at 60fps scale 1");
  check(near(graphics::get_time(), 0.0), "time starts at 0");
  step_frames(10);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  check(near(graphics::get_time(), expected_time(10, 1.0f)),
        "time is 10*dt at scale 1 regardless of wall sleep");
  check(near(graphics::get_time(), expected_time(10, 1.0f)),
        "time does not advance without frames");
  check(near(static_cast<double>(graphics::get_delta_time()),
             static_cast<double>((1.0f / 60.0f) * 1.0f)),
        "delta time matches simulated step at scale 1");
  graphics::shutdown();

  cfg.time_scale = 2.0f;
  check(graphics::init(cfg), "headless re-init at scale 2");
  check(near(graphics::get_time(), 0.0), "time resets to 0 on re-init");
  step_frames(5);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  check(near(graphics::get_time(), expected_time(5, 2.0f)),
        "time is 5*dt at scale 2 regardless of wall sleep");
  graphics::shutdown();

  printf("headless_time_test: %d/%d checks passed\n", checks_passed,
         checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
