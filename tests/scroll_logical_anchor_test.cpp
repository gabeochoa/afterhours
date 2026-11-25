#define FMT_HEADER_ONLY
#include <fmt/format.h>

#include "ui_test_harness.h"

#include <cstdio>
#include <vector>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

static int checks_run = 0;
static int checks_passed = 0;

static void check(bool cond, const char *what) {
  checks_run++;
  if (cond) {
    checks_passed++;
  } else {
    fprintf(stderr, "  FAIL: %s\n", what);
  }
}

static bool near(float a, float b) { return std::fabs(a - b) < 0.51f; }

constexpr float ROW_H = 20.f;

static void build_and_measure(ImmTestHarness &h,
                              const std::vector<size_t> &items) {
  h.begin_frame();
  auto &ctx = h.context();
  auto view = imm::div(
      ctx, imm::mk(h.root()),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(200.f), pixels(100.f)})
          .with_overflow(Overflow::Scroll, Axis::Y)
          .with_flex_direction(FlexDirection::Column)
          .with_debug_name("logical_view"));
  view.ent().get<HasScrollView>().anchor_scroll = true;
  for (size_t pos = 0; pos < items.size(); pos++) {
    imm::div(ctx,
             imm::mk_keyed(view.ent(), static_cast<EntityID>(pos),
                           items[pos] + 1),
             ComponentConfig{}
                 .with_size(ComponentSize{percent(1.f), pixels(ROW_H)})
                 .with_debug_name("logical_row"));
  }
  h.layout_only();

  MeasureScrollViews measure;
  measure.for_each_with(view.ent(), view.ent().get<HasScrollView>(),
                        view.ent().get<UIComponent>(), 0.f);
}

static HasScrollView &view_of(ImmTestHarness &h) {
  for (const auto &e : h.coll.get_entities())
    if (e && e->has<HasScrollView>())
      return e->get<HasScrollView>();
  static HasScrollView dummy;
  return dummy;
}

static std::vector<size_t> range(size_t first, size_t count) {
  std::vector<size_t> v;
  for (size_t i = 0; i < count; i++)
    v.push_back(first + i);
  return v;
}

static void settle_at(ImmTestHarness &h, const std::vector<size_t> &items,
                      float offset) {
  build_and_measure(h, items);
  auto &sv = view_of(h);
  sv.scroll_offset.y = offset;
  sv.scroll_target.y = offset;
  sv.last_eased_offset = sv.scroll_offset;
  build_and_measure(h, items);
}

static void test_prepend_holds_item_at_same_screen_y() {
  ImmTestHarness h;
  settle_at(h, range(5, 20), 5 * ROW_H + 7.f);

  build_and_measure(h, range(0, 25));

  check(near(view_of(h).scroll_offset.y, 10 * ROW_H + 7.f),
        "prepend: offset follows the pinned item, intra-item offset kept");
}

static void test_changes_below_leave_offset_alone() {
  ImmTestHarness h;
  settle_at(h, range(5, 20), 5 * ROW_H + 7.f);

  build_and_measure(h, range(5, 30));
  check(near(view_of(h).scroll_offset.y, 5 * ROW_H + 7.f),
        "append below: offset unchanged");

  build_and_measure(h, range(5, 12));
  check(near(view_of(h).scroll_offset.y, 5 * ROW_H + 7.f),
        "remove below: offset unchanged");
}

static void test_deleted_anchor_falls_back_to_next_item() {
  ImmTestHarness h;
  settle_at(h, range(5, 20), 5 * ROW_H + 7.f);

  auto without_anchor = range(5, 5);
  for (size_t i = 11; i < 25; i++)
    without_anchor.push_back(i);
  build_and_measure(h, without_anchor);
  check(near(view_of(h).scroll_offset.y, 5 * ROW_H + 7.f),
        "deleted anchor: offset kept as fallback");

  auto prepended = range(0, 5);
  for (size_t i : without_anchor)
    prepended.push_back(i);
  build_and_measure(h, prepended);
  check(near(view_of(h).scroll_offset.y, 10 * ROW_H + 7.f),
        "deleted anchor: fallback item is pinned for the next prepend");
}

int main() {
  printf("Running scroll logical anchor tests...\n\n");
  printf("  prepend_holds_item_at_same_screen_y\n");
  test_prepend_holds_item_at_same_screen_y();
  printf("  changes_below_leave_offset_alone\n");
  test_changes_below_leave_offset_alone();
  printf("  deleted_anchor_falls_back_to_next_item\n");
  test_deleted_anchor_falls_back_to_next_item();

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("SOME TESTS FAILED\n");
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
