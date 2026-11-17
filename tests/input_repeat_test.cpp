#define AFTER_HOURS_USE_NONE 1
#define FMT_HEADER_ONLY
#include <afterhours/src/plugins/input_system.h>
#include <cassert>
#include <cstdio>

using afterhours::input;

static int fires_over_one_second(int fps) {
  input::Repeat repeat{.delay = 0.25f, .rate = 0.05f};
  const float dt = 1.f / static_cast<float>(fps);
  int fires = 0;
  for (int frame = 0; frame < fps; ++frame)
    fires += repeat.tick(true, dt);
  return fires;
}

int main() {
  input::Repeat repeat{.delay = 0.25f, .rate = 0.05f};
  assert(repeat.tick(true, 1.f / 60.f) == 1);
  assert(repeat.tick(true, 0.1f) == 0);
  assert(repeat.tick(true, 0.2f) == 1);
  assert(repeat.tick(false, 0.f) == 0);
  assert(repeat.tick(true, 1.f / 60.f) == 1);

  input::Repeat stall{.delay = 0.f, .rate = 0.1f};
  stall.tick(true, 0.f);
  assert(stall.tick(true, 0.35f) == 4);

  const int at60 = fires_over_one_second(60);
  const int at200 = fires_over_one_second(200);
  std::printf("one second held: %d fires at 60 fps, %d at 200 fps\n", at60, at200);
  assert(at60 >= 16 && at60 <= 17);
  assert(at200 >= 16 && at200 <= 17);
  return 0;
}
