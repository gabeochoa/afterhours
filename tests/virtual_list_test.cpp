// virtual_list_test.cpp
// virtual_list divided by one row_height, so a list of measured rows could not
// use it. hanabi hand-rolled the same window three times rather than fight it,
// and a 2,000-row sidebar built 2,000 rows to show nineteen.
//
// The point of a windowed list is that it does NOT build what you cannot see,
// so that is what these assert: how many rows got built, not how they look.

#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

namespace {
// Rows alternate short and tall, so a uniform assumption cannot be right for
// both and the window has to come from the running total.
float alternating_height(size_t i) { return (i % 2 == 0) ? 10.f : 40.f; }
} // namespace

TEST(uniform_rows_build_only_the_window) {
  ImmTestHarness h;
  int built = 0;
  virtual_list(
      h.context(), mk(h.root(), 0), 2000, 20.f,
      [&](size_t, Entity &) { built++; },
      ComponentConfig{}
          .with_size(ComponentSize{pixels(300), pixels(200)})
          .with_debug_name("vl"));
  h.layout_only();

  printf("  uniform: built %d of 2000\n", built);
  CHECK(built > 0);
  CHECK(built < 100); // a window, not the list
}

TEST(measured_rows_build_only_the_window) {
  ImmTestHarness h;
  int built = 0;
  virtual_list(
      h.context(), mk(h.root(), 0), 2000, alternating_height,
      [&](size_t, Entity &) { built++; },
      ComponentConfig{}
          .with_size(ComponentSize{pixels(300), pixels(200)})
          .with_debug_name("vl_measured"));
  h.layout_only();

  printf("  measured: built %d of 2000\n", built);
  CHECK(built > 0);
  CHECK(built < 100);
}

// The rows it does build have to be their own heights, or the window is right
// and the list still looks wrong.
TEST(measured_rows_get_their_own_heights) {
  ImmTestHarness h;
  std::vector<float> heights;
  virtual_list(
      h.context(), mk(h.root(), 0), 12, alternating_height,
      [&](size_t i, Entity &row) {
        (void)i;
        heights.push_back(row.get<UIComponent>().desired[Axis::Y].value);
      },
      ComponentConfig{}
          .with_size(ComponentSize{pixels(300), pixels(200)})
          .with_debug_name("vl_heights"));
  h.layout_only();

  CHECK(heights.size() >= 2);
  bool saw_short = false, saw_tall = false;
  for (float ht : heights) {
    saw_short |= (ht == 10.f);
    saw_tall |= (ht == 40.f);
  }
  printf("  heights: %zu rows, short=%d tall=%d\n", heights.size(),
         (int)saw_short, (int)saw_tall);
  CHECK(saw_short);
  CHECK(saw_tall);
}

int main() { return ui_test::run_registered_tests("virtual_list"); }
