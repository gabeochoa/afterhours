#include <afterhours/src/plugins/animation_presets.h>

#include <cmath>
#include <cstdio>
#include <string>

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

int main() {
  printf("Running animation presets tests...\n\n");
  const auto fu = presets::fade_up(12.f, 0.5f, 0.1f);
  check(fu.trigger == ui_motion::MotionTrigger::Appear && fu.props.translate_y.from == 12.f &&
            fu.props.opacity.to == 1.f && fu.delay == 0.1f,
        "fade_up is an appear rule with the given distance and delay");
  check(std::holds_alternative<motion::Timeline>(fu.mode) &&
            std::get<motion::Timeline>(fu.mode).length() == 0.5f,
        "fade_up runs on a timeline of the given length");
  check(presets::hover_lift().trigger == ui_motion::MotionTrigger::Hover &&
            presets::press_squash().trigger == ui_motion::MotionTrigger::Press &&
            presets::pop_in().trigger == ui_motion::MotionTrigger::Appear,
        "hover_lift, press_squash and pop_in use their triggers");
  const auto sh = presets::shake(0.28f);
  check(std::fabs(sh.at(0.08f) - 1.f) < 1e-3f && std::fabs(sh.at(0.16f) + 1.f) < 1e-3f &&
            std::fabs(sh.at(0.28f)) < 1e-6f,
        "shake hits +1, -1 and returns to 0");
  check(presets::spin().repeat == motion::Timeline::Repeat::Loop &&
            presets::pulse().repeat == motion::Timeline::Repeat::PingPong,
        "spin loops and pulse ping-pongs");

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
