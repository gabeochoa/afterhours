// e2e_visibility_test.cpp
// E2E targets resolve to the part of an element that is actually on screen.
//
// A row inside a scroll view inside another scroll view has to accumulate
// both offsets, and then be cut by both viewports. was_rendered_to_screen
// stays true for a row scrolled out of view, so click_ui used to click the
// centre of an invisible row and land on whatever was drawn there instead.
#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using namespace afterhours::testing;
using namespace afterhours::testing::ui_commands;
using ui_test::ImmTestHarness;
using ui_test::TestInputAction;

namespace {

constexpr float ROW_H = 20.f;

// outer: 300x200 scroll view at the origin, scrolled 30
//   header: 40 tall
//   inner: 300x100 scroll view, scrolled 50
//     row_0..row_19, each 20 tall, labelled "label N"
// So the inner viewport is on screen at y 10..110, and row k is drawn at
// y = 40 - 30 + 20k - 50 = 20k - 40.
struct NestedScroll {
  ImmTestHarness h;

  NestedScroll() {
    h.begin_frame();
    auto &ctx = h.context();
    auto outer = imm::div(
        ctx, imm::mk(h.root(), 0),
        ComponentConfig{}
            .with_size(ComponentSize{pixels(300.f), pixels(200.f)})
            .with_overflow(Overflow::Scroll, Axis::Y)
            .with_flex_direction(FlexDirection::Column)
            .with_debug_name("outer"));
    imm::div(ctx, imm::mk(outer.ent(), 0),
             ComponentConfig{}
                 .with_size(ComponentSize{pixels(300.f), pixels(40.f)})
                 .with_debug_name("header"));
    auto inner = imm::div(
        ctx, imm::mk(outer.ent(), 1),
        ComponentConfig{}
            .with_size(ComponentSize{pixels(300.f), pixels(100.f)})
            .with_overflow(Overflow::Scroll, Axis::Y)
            .with_flex_direction(FlexDirection::Column)
            .with_debug_name("inner"));
    for (int i = 0; i < 20; ++i)
      imm::div(ctx, imm::mk(inner.ent(), i),
               ComponentConfig{}
                   .with_size(ComponentSize{pixels(300.f), pixels(ROW_H)})
                   .with_label("label " + std::to_string(i))
                   .with_debug_name("row_" + std::to_string(i)));
    h.layout_only();
    outer.ent().get<HasScrollView>().scroll_offset = {0.f, 30.f};
    inner.ent().get<HasScrollView>().scroll_offset = {0.f, 50.f};
    test_input::reset_all();
  }
  ~NestedScroll() { test_input::reset_all(); }

  Entity &row(int i) {
    for (const auto &e : h.coll.get_entities())
      if (e && e->has<UIComponentDebug>() &&
          e->get<UIComponentDebug>().name() == "row_" + std::to_string(i))
        return *e;
    static Entity none;
    return none;
  }

  template <typename Handler>
  PendingE2ECommand run(const std::string &name,
                        std::vector<std::string> args) {
    PendingE2ECommand cmd;
    cmd.name = name;
    cmd.args = std::move(args);
    Handler handler;
    handler.for_each_with(row(0), cmd, 0.f);
    return cmd;
  }
};

bool contains(const std::string &s, const std::string &needle) {
  return s.find(needle) != std::string::npos;
}

} // namespace

TEST(screen_rect_accumulates_every_ancestor_scroll) {
  NestedScroll f;
  for (int k : {0, 2, 5, 7}) {
    const RectangleType r = get_screen_rect(f.row(k));
    CHECK_APPROX(r.y, 20.f * k - 40.f);
    CHECK_APPROX(r.height, ROW_H);
  }
}

TEST(visible_rect_is_cut_by_both_viewports) {
  NestedScroll f;
  CHECK(!is_visible(f.row(0)));  // y -40..-20, above both viewports
  CHECK(!is_visible(f.row(1)));  // y -20..0, above the inner viewport
  CHECK(is_visible(f.row(2)));   // y 0..20, bottom half shows
  CHECK_APPROX(get_visible_rect(f.row(2)).y, 10.f);
  CHECK_APPROX(get_visible_rect(f.row(2)).height, 10.f);
  CHECK_APPROX(get_visible_rect(f.row(5)).y, 60.f);
  CHECK_APPROX(get_visible_rect(f.row(5)).height, ROW_H);
  CHECK_APPROX(get_visible_rect(f.row(7)).height, 10.f);  // 100..110
  CHECK(!is_visible(f.row(8)));  // y 120..140, below the inner viewport

  // The same rectangle real hit-testing uses, so a scripted click and a
  // real one agree about what is under the pointer.
  Entity &partial = f.row(2);
  const RectangleType hit =
      ui::detail::hit_rect(partial, partial.get<UIComponent>());
  CHECK_APPROX(hit.y, get_visible_rect(partial).y);
  CHECK_APPROX(hit.height, get_visible_rect(partial).height);
}

TEST(click_ui_aims_at_the_visible_portion) {
  NestedScroll f;
  auto cmd =
      f.run<HandleClickUICommand<TestInputAction>>("click_ui", {"row_2"});
  CHECK(cmd.is_consumed() && cmd.succeeded());
  // The full-rect centre (y 10) sits on the clip edge; the visible centre
  // is y 15.
  CHECK_APPROX(input_injector::detail::mouse.pos.y, 15.f);
}

TEST(click_commands_fail_on_a_fully_clipped_target) {
  NestedScroll f;
  auto by_name =
      f.run<HandleClickUICommand<TestInputAction>>("click_ui", {"row_0"});
  CHECK(!by_name.succeeded());
  CHECK(contains(by_name.error_message, "fully clipped"));
  CHECK(contains(by_name.error_message, "row_0"));

  auto by_text = f.run<HandleClickTextCommand<TestInputAction>>(
      "click_text", {"label 8"});
  CHECK(!by_text.succeeded());
  CHECK(contains(by_text.error_message, "fully clipped"));

  auto right = f.run<HandleRightClickUICommand<TestInputAction>>(
      "right_click_ui", {"row_1"});
  CHECK(!right.succeeded());

  auto dbl = f.run<HandleMultiClickUICommand<TestInputAction, 2>>(
      "double_click_ui", {"row_0", "center", "center"});
  CHECK(!dbl.succeeded());
  CHECK(contains(dbl.error_message, "clipped"));

  // An absent target still says "not found", not "clipped".
  auto missing =
      f.run<HandleClickUICommand<TestInputAction>>("click_ui", {"row_99"});
  CHECK(contains(missing.error_message, "not found"));
  CHECK(input_injector::detail::mouse.left_down == false);
}

TEST(assert_ui_reports_visibility_and_the_visible_rect) {
  NestedScroll f;
  CHECK(check_ui_property(f.row(0), "visible", "false").empty());
  CHECK(check_ui_property(f.row(5), "visible", "true").empty());
  CHECK(!check_ui_property(f.row(0), "visible", "true").empty());
  CHECK(check_ui_property(f.row(2), "vy", "10").empty());
  CHECK(check_ui_property(f.row(2), "vh", "10").empty());
  CHECK(check_ui_property(f.row(2), "y", "0").empty());
  CHECK(check_ui_property(f.row(2), "h", "20").empty());
}

TEST(dump_ui_marks_clipped_nodes_only) {
  NestedScroll f;
  std::string out;
  dump_ui_node(out, f.row(0), 0);
  CHECK(contains(out, "visible=\"false\""));
  out.clear();
  dump_ui_node(out, f.row(2), 0);
  CHECK(contains(out, "visible=\"partial\""));
  CHECK(contains(out, "vy=\"10\" vw=\"300\" vh=\"10\""));
  out.clear();
  dump_ui_node(out, f.row(5), 0);
  CHECK(!contains(out, "visible="));
}

int main() { return ui_test::run_registered_tests("E2E clipped visibility"); }
