// on_draw_over: a container paints after its subtree (hanabi #63), unlike
// on_draw_fg which is only after the widget's own primitives.
#include "ui_test_harness.h"

#include <algorithm>
#include <string>
#include <vector>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

static std::vector<std::string> build_and_render(bool batched) {
  ImmTestHarness h;
  std::vector<std::string> order;
  auto outer = div(h.context(), mk(h.root(), 0),
                   ComponentConfig{}
                       .with_debug_name("over_outer")
                       .with_on_draw_fg([&](RectangleType) { order.push_back("outer_fg"); })
                       .with_on_draw_over([&](RectangleType) { order.push_back("outer_over"); }));
  auto mid = div(h.context(), mk(outer.ent(), 0),
                 ComponentConfig{}
                     .with_debug_name("over_mid")
                     .with_on_draw_fg([&](RectangleType) { order.push_back("mid_fg"); })
                     .with_on_draw_over([&](RectangleType) { order.push_back("mid_over"); }));
  div(h.context(), mk(mid.ent(), 0),
      ComponentConfig{}
          .with_debug_name("over_leaf")
          .with_on_draw_fg([&](RectangleType) { order.push_back("leaf_fg"); }));
  div(h.context(), mk(outer.ent(), 1),
      ComponentConfig{}
          .with_debug_name("over_sibling")
          .with_on_draw_fg([&](RectangleType) { order.push_back("sibling_fg"); }));
  if (batched) h.render_batched(); else h.render();
  return order;
}

static int pos(const std::vector<std::string> &v, const std::string &s) {
  auto it = std::find(v.begin(), v.end(), s);
  return it == v.end() ? -1 : static_cast<int>(it - v.begin());
}

static void check_both(bool batched) {
  const auto order = build_and_render(batched);
  CHECK(order.size() == 6);
  // fg stays where it was: before children.
  CHECK(pos(order, "outer_fg") < pos(order, "mid_fg"));
  CHECK(pos(order, "mid_fg") < pos(order, "leaf_fg"));
  // over is after the whole subtree it owns.
  CHECK(pos(order, "leaf_fg") < pos(order, "mid_over"));
  CHECK(pos(order, "mid_over") < pos(order, "sibling_fg"));
  CHECK(pos(order, "sibling_fg") < pos(order, "outer_over"));
  CHECK(pos(order, "mid_over") < pos(order, "outer_over"));
}

TEST(draw_over_batched) { check_both(true); }
TEST(draw_over_immediate) { check_both(false); }

TEST(draw_over_default_unchanged) {
  ImmTestHarness h;
  std::vector<std::string> order;
  auto parent = div(h.context(), mk(h.root(), 0),
                    ComponentConfig{}
                        .with_debug_name("plain_parent")
                        .with_on_draw_fg([&](RectangleType) { order.push_back("parent_fg"); }));
  div(h.context(), mk(parent.ent(), 0),
      ComponentConfig{}
          .with_debug_name("plain_child")
          .with_on_draw_fg([&](RectangleType) { order.push_back("child_fg"); }));
  h.render();
  CHECK(order.size() == 2);
  CHECK(pos(order, "parent_fg") < pos(order, "child_fg"));
  // Over-only config still creates HasOnDraw and merges like fg.
  ComponentConfig merged = ComponentConfig{}.apply_overrides(
      ComponentConfig{}.with_on_draw_over([](RectangleType) {}));
  CHECK(static_cast<bool>(merged.on_draw_over));
}

int main() { return ui_test::run_registered_tests("draw over tests"); }
