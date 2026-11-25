#include <afterhours/src/window_events.h>

#include <cstdio>
#include <string>

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
  using namespace afterhours::window_events;
  clear();

  check(take().empty(), "no event -> no spurious fire");
  check(!occluded(), "not occluded at startup");
  check(last_scale() == 0.f, "no scale reported yet");

  push({Type::Resized, 800, 600, 1.f});
  auto evs = take();
  check(evs.size() == 1 && evs[0].type == Type::Resized && evs[0].width == 800,
        "injected resize observed by app-side hook");
  check(take().empty(), "take drains the queue");

  push({Type::ScaleChanged, 800, 600, 2.f});
  evs = take();
  check(evs.size() == 1 && evs[0].type == Type::ScaleChanged &&
            evs[0].scale == 2.f,
        "injected scale change observed with new scale");
  check(last_scale() == 2.f, "last_scale polls the new backing scale");

  push({Type::Occluded, 800, 600, 2.f});
  check(occluded(), "occluded is pollable before take");
  evs = take();
  check(evs.size() == 1 && evs[0].type == Type::Occluded, "occlusion event");
  push({Type::Revealed, 800, 600, 2.f});
  check(!occluded(), "revealed clears pollable occlusion");
  take();

  push({Type::Suspended, 0, 0, 0.f});
  check(occluded(), "suspend counts as occluded");
  push({Type::Resumed, 0, 0, 0.f});
  check(!occluded(), "resume clears occlusion");
  check(take().size() == 2, "suspend/resume both queued in order");

  clear();
  check(take().empty() && !occluded() && last_scale() == 0.f, "clear resets");

  printf("window_events: %d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
