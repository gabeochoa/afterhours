// e2e_zoom_coordinates_test.cpp
// E2E coordinates are logical pixels at every UI scale.
//
// Adaptive scaling multiplies layout by ui_scale, so a widget written as
// 100x30 at y 40 is 140x42 at y 56 on screen at 140%. Scripts used to read
// and write screen pixels, so every coordinate and every assert_ui went stale
// the moment the user zoomed. Here the same script lines hold at 1.0, 1.4 and
// 2.0, and the screen rect is checked too, so the scale really applied.
#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using namespace afterhours::testing;
using namespace afterhours::testing::ui_commands;
using ui_test::ImmTestHarness;
using ui_test::TestInputAction;

namespace {

struct ScaleScope {
  Theme saved = ThemeDefaults::get().theme;
  ScalingMode saved_mode = UIStylingDefaults::get().scaling_mode;
  ScaleScope(float scale, ScalingMode mode) {
    Theme t = saved;
    t.ui_scale = scale;
    ThemeDefaults::get().set_theme(t);
    UIStylingDefaults::get().scaling_mode = mode;
  }
  ~ScaleScope() {
    ThemeDefaults::get().set_theme(saved);
    UIStylingDefaults::get().scaling_mode = saved_mode;
  }
};

// A 40-tall spacer, then "target" at logical (0,40) 100x30.
Entity &build(ImmTestHarness &h, float scale) {
  h.begin_frame();
  auto &ctx = h.context();
  auto col = imm::div(ctx, imm::mk(h.root(), 0),
                      ComponentConfig{}
                          .with_size(ComponentSize{pixels(400.f), pixels(300.f)})
                          .with_flex_direction(FlexDirection::Column));
  imm::div(ctx, imm::mk(col.ent(), 0),
           ComponentConfig{}.with_size(
               ComponentSize{pixels(100.f), pixels(40.f)}));
  auto target = imm::div(ctx, imm::mk(col.ent(), 1),
                         ComponentConfig{}
                             .with_size(ComponentSize{pixels(100.f),
                                                      pixels(30.f)})
                             .with_debug_name("target"));
  h.layout_only(false, {800, 600}, scale);
  return target.ent();
}

PendingE2ECommand run(System<PendingE2ECommand> &&handler, Entity &e,
                      const std::string &name, std::vector<std::string> args) {
  PendingE2ECommand cmd;
  cmd.name = name;
  cmd.args = std::move(args);
  handler.for_each_with(e, cmd, 0.f);
  return cmd;
}

} // namespace

TEST(coordinates_are_logical_at_every_scale) {
  for (float scale : {1.0f, 1.4f, 2.0f}) {
    printf("    scale %.1f\n", scale);
    ScaleScope scope(scale, ScalingMode::Adaptive);
    ImmTestHarness h;
    Entity &target = build(h, scale);
    test_input::reset_all();

    // Not vacuous: on screen the widget really did scale.
    const RectangleType screen = get_screen_rect(target);
    CHECK_APPROX(screen.y, 40.f * scale);
    CHECK_APPROX(screen.height, 30.f * scale);
    CHECK_APPROX(e2e_ui_scale(), scale);

    // Reports: the same numbers at every scale.
    for (const char *prop : {"x=0", "y=40", "w=100", "h=30", "vy=40", "vh=30"}) {
      std::string p, v;
      parse_prop_assertion(prop, p, v);
      const std::string err = check_ui_property(target, p, v);
      CHECK(err.empty());
      if (!err.empty()) printf("      %s\n", err.c_str());
    }
    std::string xml;
    dump_ui_node(xml, target, 0);
    CHECK(xml.find("x=\"0\" y=\"40\" w=\"100\" h=\"30\"") != std::string::npos);

    // Input: a logical point lands inside the widget's screen rect.
    auto click = run(HandleClickCommand{}, target, "click", {"50", "55"});
    CHECK(click.succeeded());
    const auto &m = input_injector::detail::mouse;
    CHECK_APPROX(m.pos.x, 50.f * scale);
    CHECK_APPROX(m.pos.y, 55.f * scale);
    CHECK(m.pos.y > screen.y && m.pos.y < screen.y + screen.height);

    // Percentages stay fractions of the window.
    run(HandleMouseMoveCommand{}, target, "mouse_move", {"50%", "50%"});
    const auto [sw, sh] = e2e_screen_size();
    CHECK_APPROX(m.pos.x, sw / 2);
    CHECK_APPROX(m.pos.y, sh / 2);

    // Offsets from a named element are logical too.
    multi_click_ui_detail::reset();
    run(HandleMultiClickUICommand<TestInputAction, 2>{}, target,
        "double_click_ui", {"target", "10", "5"});
    CHECK_APPROX(m.pos.x, screen.x + 10.f * scale);
    CHECK_APPROX(m.pos.y, screen.y + 5.f * scale);
    multi_click_ui_detail::reset();
    test_input::reset_all();
  }
}

TEST(proportional_mode_ignores_ui_scale_like_layout_does) {
  ScaleScope scope(2.0f, ScalingMode::Proportional);
  ImmTestHarness h;
  Entity &target = build(h, 2.0f);
  CHECK_APPROX(e2e_ui_scale(), 1.f);
  CHECK_APPROX(get_screen_rect(target).height, 30.f);
  CHECK(check_ui_property(target, "h", "30").empty());
}

int main() { return ui_test::run_registered_tests("E2E zoom coordinates"); }
