// tab_container_test.cpp
// Regression tests for the imm::tab_container widget.
//
// Build (from the afterhours repo root):
//   clang++ -std=c++23 -I.. -Ivendor examples/tab_container_test.cpp -o /tmp/t && /tmp/t
//
// Covers the equal-1/N-width truncation bug: tabs used percent(1/N) widths, so
// a label longer than its slice ellipsized even when the bar had room and
// shorter tabs had slack. Tabs now use expand() width with a min-width of their
// own label (Dim::Text content-fit), so a tab never shrinks below its content.

#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

// A tab whose label is longer than the equal 1/N slice grows to fit its text.
TEST(tab_container_long_label_not_truncated) {
  ImmTestHarness h;
  std::vector<std::string> tabs = {"A", "B", "LongCategoryName", "C", "D"};
  size_t active = 0;

  tab_container(h.context(), mk(h.root(), 0), tabs, active,
                ComponentConfig{}
                    .with_size(ComponentSize{pixels(800), pixels(44)})
                    .with_debug_name("tabs"));
  h.layout_and_render();

  UIComponent *long_tab = h.find("tab_2"); // the "LongCategoryName" tab
  CHECK(long_tab != nullptr);
  if (long_tab) {
    float equal_slice = 800.f / 5.f; // the old fixed width
    // Content-fit: the long tab is wider than the equal slice.
    CHECK(long_tab->rect().width > equal_slice + 1.f);
  }
}

// Short, equal-length labels still distribute evenly (no visible change).
TEST(tab_container_equal_labels_even_split) {
  ImmTestHarness h;
  std::vector<std::string> tabs = {"One", "Two", "Ten"};
  size_t active = 0;

  tab_container(h.context(), mk(h.root(), 0), tabs, active,
                ComponentConfig{}
                    .with_size(ComponentSize{pixels(600), pixels(44)})
                    .with_debug_name("tabs"));
  h.layout_and_render();

  UIComponent *t0 = h.find("tab_0");
  UIComponent *t1 = h.find("tab_1");
  UIComponent *t2 = h.find("tab_2");
  CHECK(t0 && t1 && t2);
  if (t0 && t1 && t2) {
    // Equal-width labels share the bar equally.
    CHECK_APPROX(t0->rect().width, t1->rect().width);
    CHECK_APPROX(t1->rect().width, t2->rect().width);
  }
}

int main() { return ui_test::run_registered_tests("tab_container tests"); }
