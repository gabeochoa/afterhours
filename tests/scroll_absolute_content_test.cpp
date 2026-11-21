// Absolute children of a scroll view are content at their position:
// a child placed beyond the flow must extend the scrollable extent to
// its far edge (not just add its size), an overlay inside the flow must
// not grow the extent, and the child must move with the scroll offset.
#define FMT_HEADER_ONLY
#include <fmt/format.h>

#include "ui_test_harness.h"

#include <cstdio>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

static int checks_run = 0;
static int checks_passed = 0;
static void check(bool cond, const char *what) {
  checks_run++;
  if (cond) checks_passed++;
  else fprintf(stderr, "  FAIL: %s\n", what);
}

static bool near(float a, float b) { return std::fabs(a - b) < 0.51f; }

struct Built {
  Entity *view = nullptr;
  Entity *abs_child = nullptr;
};

// flow: 5 rows x 20px in a 200x100 column scroll view; optionally one
// absolute child.
static Built build(ImmTestHarness &h, bool with_abs, float abs_x, float abs_y,
                   float abs_w, float abs_h, bool overlay_only = false) {
  h.begin_frame();
  auto &ctx = h.context();
  auto view = imm::div(ctx, imm::mk(h.root()),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(200.f), pixels(100.f)})
          .with_overflow(Overflow::Scroll, Axis::Y)
          .with_flex_direction(FlexDirection::Column)
          .with_debug_name("scroll_view"));
  Built out{&view.ent(), nullptr};
  if (!overlay_only) {
    for (int i = 0; i < 5; i++) {
      imm::div(ctx, imm::mk(view.ent(), 100 + i),
               ComponentConfig{}
                   .with_size(ComponentSize{pixels(100.f), pixels(20.f)})
                   .with_debug_name("flow_row"));
    }
  }
  if (with_abs) {
    auto a = imm::div(ctx, imm::mk(view.ent(), 999),
        ComponentConfig{}
            .with_size(ComponentSize{pixels(abs_w), pixels(abs_h)})
            .with_absolute_position(abs_x, abs_y)
            .with_debug_name("abs_child"));
    out.abs_child = &a.ent();
  }
  h.layout_only();
  MeasureScrollViews measure;
  measure.for_each_with(view.ent(), view.ent().get<HasScrollView>(),
                        view.ent().get<UIComponent>(), 0.f);
  return out;
}

static void test_absolute_beyond_flow_extends_extent() {
  ImmTestHarness h;
  auto b = build(h, true, 0.f, 300.f, 50.f, 50.f);
  auto &sv = b.view->get<HasScrollView>();
  printf("  [measure] beyond: content=(%.1f,%.1f) viewport=(%.1f,%.1f) max_y=%.1f\n",
         sv.content_size.x, sv.content_size.y, sv.viewport_or_zero().x,
         sv.viewport_or_zero().y, sv.bottom_offset());
  check(near(sv.content_size.y, 350.f),
        "extent reaches absolute child's far edge (300+50)");
  check(near(sv.bottom_offset(), 250.f), "max offset = 350-100 viewport");
}

static void test_absolute_overlay_does_not_grow_extent() {
  ImmTestHarness h;
  auto b = build(h, true, 0.f, 0.f, 200.f, 100.f);
  auto &sv = b.view->get<HasScrollView>();
  printf("  [measure] overlay: content=(%.1f,%.1f)\n", sv.content_size.x,
         sv.content_size.y);
  check(near(sv.content_size.y, 100.f),
        "overlay inside flow extent does not grow content");
}

static void test_absolute_cross_axis_position_counts() {
  ImmTestHarness h;
  auto b = build(h, true, 150.f, 300.f, 50.f, 50.f);
  auto &sv = b.view->get<HasScrollView>();
  printf("  [measure] cross: content=(%.1f,%.1f)\n", sv.content_size.x,
         sv.content_size.y);
  check(near(sv.content_size.x, 200.f),
        "cross extent reaches absolute child's right edge (150+50)");
}

static void test_absolute_child_moves_with_scroll() {
  ImmTestHarness h;
  auto b = build(h, true, 0.f, 300.f, 50.f, 50.f);
  auto &sv = b.view->get<HasScrollView>();
  const auto &cmp = b.abs_child->get<UIComponent>();
  RectangleType base = cmp.rect();
  auto at0 = ::afterhours::ui::detail::apply_ancestor_transform(*b.abs_child, base);
  sv.scroll_offset.y = 40.f;
  sv.scroll_target.y = 40.f;
  auto at40 = ::afterhours::ui::detail::apply_ancestor_transform(*b.abs_child, base);
  printf("  [measure] move: rect.y=%.1f at0.y=%.1f at40.y=%.1f\n", base.y,
         at0.y, at40.y);
  check(near(at0.y, base.y), "no offset: transformed y equals layout y");
  check(near(at40.y, base.y - 40.f), "scroll offset shifts absolute child");
}

static void test_flow_only_unchanged() {
  ImmTestHarness h;
  auto b = build(h, false, 0, 0, 0, 0);
  auto &sv = b.view->get<HasScrollView>();
  printf("  [measure] flow-only: content=(%.1f,%.1f)\n", sv.content_size.x,
         sv.content_size.y);
  check(near(sv.content_size.y, 100.f), "flow-only content height unchanged");
  check(near(sv.content_size.x, 100.f), "flow-only content width unchanged");
  check(near(sv.bottom_offset(), 0.f), "flow-only max offset unchanged");
}

int main() {
  printf("Running scroll absolute content tests...\n\n");
  test_absolute_beyond_flow_extends_extent();
  test_absolute_overlay_does_not_grow_extent();
  test_absolute_cross_axis_position_counts();
  test_absolute_child_moves_with_scroll();
  test_flow_only_unchanged();
  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) { printf("SOME TESTS FAILED\n"); return 1; }
  printf("All checks passed!\n");
  return 0;
}
