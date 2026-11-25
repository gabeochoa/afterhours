#define AFTER_HOURS_USE_NONE
#include <afterhours/src/plugins/texture_manager.h>

#include <cmath>
#include <cstdio>
#include <string>

using namespace afterhours;

static int checks_run = 0;
static int checks_passed = 0;
static void check(bool cond, const std::string &what) {
  checks_run++;
  if (cond) checks_passed++;
  else fprintf(stderr, "  FAIL: %s\n", what.c_str());
}

static int frames_in_one_second(float fps) {
  Entity &e = EntityHelper::createEntity();
  EntityHelper::merge_entity_arrays();
  texture_manager::HasAnimation::Params params;
  params.total_frames = 100000;
  params.frame_dur = 1.f / 24.f;
  auto &anim = e.addComponent<texture_manager::HasAnimation>(params);
  texture_manager::AnimationUpdateCurrentFrame sys;
  const int steps = static_cast<int>(fps);
  for (int i = 0; i < steps; ++i) sys.for_each_with(e, anim, 1.f / fps);
  const int frames = anim.cur_frame();
  e.cleanup = true;
  EntityHelper::cleanup();
  return frames;
}

int main() {
  printf("Running sprite animation tests...\n\n");
  for (float fps : {60.f, 120.f, 200.f}) {
    const int frames = frames_in_one_second(fps);
    char buf[128];
    snprintf(buf, sizeof(buf), "24fps sprite advances ~24 frames in 1s at %d fps (got %d)",
             static_cast<int>(fps), frames);
    check(std::abs(frames - 24) <= 1, buf);
  }
  {
    Entity &e = EntityHelper::createEntity();
    EntityHelper::merge_entity_arrays();
    texture_manager::HasAnimation::Params params;
    params.total_frames = 100000;
    params.frame_dur = 1.f / 24.f;
    auto &anim = e.addComponent<texture_manager::HasAnimation>(params);
    texture_manager::AnimationUpdateCurrentFrame sys;
    motion::pause(true);
    for (int i = 0; i < 60; ++i) sys.for_each_with(e, anim, 1.f / 60.f);
    motion::pause(false);
    check(anim.cur_frame() == 0, "paused clock freezes sprite animation");
    e.cleanup = true;
    EntityHelper::cleanup();
  }
  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) { printf("FAILURES: %d\n", checks_run - checks_passed); return 1; }
  printf("All checks passed!\n");
  return 0;
}
