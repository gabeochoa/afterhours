#include <afterhours/src/plugins/ui/motion_config.h>

#include <cmath>
#include <cstdio>
#include <string>

using namespace afterhours;
using namespace afterhours::ui;

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

static constexpr size_t kScale = static_cast<size_t>(MotionProperty::Scale);
static constexpr size_t kOpacity =
    static_cast<size_t>(MotionProperty::Opacity);
static constexpr size_t kTx = static_cast<size_t>(MotionProperty::TranslateX);

static bool is_spring(const motion::Mode &m, float response) {
  return std::holds_alternative<motion::Spring>(m) &&
         std::fabs(std::get<motion::Spring>(m).response - response) < 1e-6f;
}

int main() {
  printf("Running motion config tests...\n\n");

  const motion::Spring hover_spring{.response = 0.11f};
  const motion::Spring press_spring{.response = 0.22f};
  const motion::Spring focus_spring{.response = 0.33f};

  {
    std::vector<MotionRule> rules{
        {MotionTrigger::Hover, {.scale = 1.05f}, hover_spring}};
    HasMotionState st;
    auto t = resolve_motion(rules, {}, st);
    check(t[kScale].set && t[kScale].value == 1.f,
          "no hover: scale targets its rest of 1");
    check(!t[kOpacity].set && !t[kTx].set,
          "properties no rule mentions are left alone");
    t = resolve_motion(rules, {.hot = true}, st);
    check(t[kScale].value == 1.05f && is_spring(t[kScale].mode, 0.11f),
          "hover on: scale targets 1.05 with the hover spring");
    t = resolve_motion(rules, {}, st);
    check(t[kScale].value == 1.f && is_spring(t[kScale].mode, 0.11f),
          "hover off: back to rest along the same spring");
  }

  {
    std::vector<MotionRule> rules{
        {MotionTrigger::Focus, {.scale = 1.02f}, focus_spring},
        {MotionTrigger::Hover, {.scale = 1.05f}, hover_spring},
        {MotionTrigger::Press, {.scale = 0.9f}, press_spring}};
    HasMotionState st;
    auto t = resolve_motion(rules, {.hot = true, .active = true, .focus = true},
                            st);
    check(t[kScale].value == 0.9f && is_spring(t[kScale].mode, 0.22f),
          "press beats hover and focus for the same property");
    t = resolve_motion(rules, {.hot = true, .focus = true}, st);
    check(t[kScale].value == 1.05f && is_spring(t[kScale].mode, 0.11f),
          "releasing press recovers the still-active hover target");
    t = resolve_motion(rules, {.focus = true}, st);
    check(t[kScale].value == 1.02f && is_spring(t[kScale].mode, 0.33f),
          "leaving hover falls through to focus");
    t = resolve_motion(rules, {}, st);
    check(t[kScale].value == 1.f && is_spring(t[kScale].mode, 0.33f),
          "nothing active: rest, released along the last winning spring");
  }

  {
    std::vector<MotionRule> rules{{MotionTrigger::Appear,
                                   {.translate_x = {-40.f, 0.f},
                                    .opacity = {0.f, 1.f}},
                                   motion::Spring::gentle(),
                                   0.25f}};
    HasMotionState st;
    auto t = resolve_motion(rules, {}, st);
    check(t[kOpacity].set && t[kOpacity].reset_to == 0.f &&
              t[kOpacity].value == 1.f && t[kOpacity].delay == 0.25f,
          "first frame: appear resets to from, targets to, carries delay");
    check(t[kTx].reset_to == -40.f && t[kTx].value == 0.f,
          "appear works per property");
    t = resolve_motion(rules, {}, st);
    check(t[kOpacity].set && !t[kOpacity].reset_to.has_value() &&
              t[kOpacity].value == 1.f && t[kOpacity].delay == 0.f,
          "later frames: appear only pins the rest value");
  }

  {
    HasMotionState st;
    std::vector<MotionRule> open{
        {MotionTrigger::State, {.scale = {0.97f, 1.f}, .opacity = 1.f},
         motion::Spring::smooth()}};
    open[0].state = true;
    auto t = resolve_motion(open, {}, st);
    check(t[kScale].value == 1.f && t[kOpacity].value == 1.f,
          "state true targets to");
    open[0].state = false;
    t = resolve_motion(open, {}, st);
    check(t[kScale].value == 0.97f && t[kOpacity].value == 1.f,
          "state false targets from when given, else rest");
  }

  {
    HasMotionState st;
    std::vector<MotionRule> pop{
        {MotionTrigger::Change, {.scale = 1.2f}, motion::Spring::bouncy()}};
    pop[0].stamp = 1;
    auto t = resolve_motion(pop, {}, st);
    check(t[kScale].value == 1.f, "first sighting of a stamp does not fire");
    t = resolve_motion(pop, {}, st);
    check(t[kScale].value == 1.f, "same stamp does not fire");
    pop[0].stamp = 2;
    t = resolve_motion(pop, {}, st);
    check(t[kScale].value == 1.2f, "a new stamp fires the impulse");
    t = resolve_motion(pop, {}, st);
    check(t[kScale].value == 1.f, "the frame after, it springs back to rest");
  }

  {
    HasMotionState st;
    std::vector<MotionRule> rules{
        {MotionTrigger::Hover, {.scale = 1.05f}, hover_spring},
        {MotionTrigger::Change, {.scale = {1.3f, 1.f}}, motion::Spring::bouncy()}};
    rules[1].stamp = 1;
    resolve_motion(rules, {.hot = true}, st);
    rules[1].stamp = 2;
    auto t = resolve_motion(rules, {.hot = true}, st);
    check(t[kScale].reset_to == 1.3f && t[kScale].value == 1.f,
          "change with a from jumps there and settles to its to, over hover");
    t = resolve_motion(rules, {.hot = true}, st);
    check(t[kScale].value == 1.05f, "then hover's target is back in charge");
  }

  {
    HasMotionState st;
    std::vector<MotionRule> rules{
        {MotionTrigger::Change, {.scale = 1.2f}, motion::Spring::bouncy()},
        {MotionTrigger::Change, {.opacity = 0.5f}, motion::Spring::bouncy()}};
    rules[0].stamp = 1;
    rules[1].stamp = 7;
    resolve_motion(rules, {}, st);
    rules[0].stamp = 2;
    auto t = resolve_motion(rules, {}, st);
    check(t[kScale].value == 1.2f && t[kOpacity].value == 1.f,
          "each change rule keeps its own stamp, so only the changed one fires");
    rules[1].stamp = 8;
    t = resolve_motion(rules, {}, st);
    check(t[kScale].value == 1.f && t[kOpacity].value == 0.5f,
          "and the other fires on its own change");
  }

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
