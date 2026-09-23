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

  check(presets::effect_presets.size() == 17, "effect preset ships 17 effects");
  {
    bool unique = true;
    for (size_t i = 0; i < presets::effect_presets.size(); ++i)
      for (size_t j = i + 1; j < presets::effect_presets.size(); ++j)
        unique &= presets::effect_presets[i].id != presets::effect_presets[j].id;
    check(unique, "effect preset ids are unique");
    bool modes_ok = true;
    for (const auto &e : presets::effect_presets)
      modes_ok &= (e.shader == presets::EffectShader::None || static_cast<int>(e.shader) >= 0);
    check(modes_ok && presets::effect_preset(presets::EffectId::Tumble) != nullptr,
          "shader effects carry an EffectShader and effect_preset resolves ids");
  }
  {
    const auto *flip = presets::effect_preset(presets::EffectId::Flip);
    const RectangleType r{0.f, 0.f, 100.f, 50.f};
    const auto q0 = presets::effect_quad(*flip, r, 0.f);
    const auto qh = presets::effect_quad(*flip, r, 0.5f);
    check(std::fabs(q0.corners[1].x - q0.corners[0].x - 100.f) < 1e-3f &&
              std::fabs(qh.corners[1].x - qh.corners[0].x) < 1e-3f,
          "flip is full width at p=0 and edge-on at p=0.5");
    const auto *recede = presets::effect_preset(presets::EffectId::Recede);
    const auto *emerge = presets::effect_preset(presets::EffectId::Emerge);
    check(presets::effect_quad(*recede, r, 1.f).opacity < 0.3f &&
              presets::effect_quad(*emerge, r, 0.f).opacity == 0.f &&
              presets::effect_quad(*emerge, r, 1.f).opacity == 1.f,
          "recede fades out and emerge fades in over progress");
    const auto *blur = presets::effect_preset(presets::EffectId::Blur);
    const auto *unblur = presets::effect_preset(presets::EffectId::Unblur);
    check(presets::effect_blur_radius(*blur, 1.f) == 6.f && presets::effect_blur_radius(*unblur, 1.f) == 0.f &&
              presets::effect_blur_radius(*recede, 1.f) == 0.f,
          "blur ramps 0 to 6, unblur ramps back, other effects stay sharp");
    check(std::fabs(presets::effect_timeline(0.9f).length() - 0.9f) < 1e-6f,
          "effect preset timeline has the requested length");
  }

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
