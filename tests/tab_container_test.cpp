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
  // Long enough to beat the 160px slice at the type scale: 16 chars used to
  // overflow only because auto-fit sized the label off the box.
  std::vector<std::string> tabs = {"A", "B", "LongCategoryNameThatOverflows",
                                   "C", "D"};
  size_t active = 0;

  tab_container(h.context(), mk(h.root(), 0), tabs, active,
                ComponentConfig{}
                    .with_size(ComponentSize{pixels(800), pixels(44)})
                    .with_debug_name("tabs"));
  h.layout_only();

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
  h.layout_only();

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

// Every tab stays inside the bar: the long tab keeps its label width and
// the short tabs give up part of their equal share to pay for it, instead
// of the long tab being grown past the bar's right edge after the split.
TEST(tab_container_tabs_stay_inside_the_bar) {
  ImmTestHarness h;
  std::vector<std::string> tabs = {"A", "B", "LongCategoryNameThatOverflows",
                                   "C", "D"};
  size_t active = 0;
  tab_container(h.context(), mk(h.root(), 0), tabs, active,
                ComponentConfig{}
                    .with_size(ComponentSize{pixels(800), pixels(44)})
                    .with_debug_name("tabs"));
  h.layout_only();

  UIComponent *bar = h.find("tabs");
  CHECK(bar != nullptr);
  if (!bar)
    return;
  const float bar_right = bar->rect().x + bar->rect().width;
  float total = 0.f;
  float short_w = -1.f;
  for (int i = 0; i < 5; ++i) {
    UIComponent *t = h.find("tab_" + std::to_string(i));
    CHECK(t != nullptr);
    if (!t)
      return;
    fprintf(stderr, "        tab_%d x=%.1f w=%.1f\n", i, t->rect().x,
            t->rect().width);
    CHECK(t->rect().x >= bar->rect().x - 0.5f);
    CHECK(t->rect().x + t->rect().width <= bar_right + 0.5f);
    total += t->rect().width;
    if (i != 2) {
      if (short_w < 0.f)
        short_w = t->rect().width;
      else
        CHECK_APPROX(t->rect().width, short_w);
    }
  }
  // The row fills the bar (no pile of slack, no overflow).
  CHECK(total <= bar->rect().width + 0.5f);
  CHECK(total >= bar->rect().width - 2.f);
}

// When the labels together are wider than the bar, the floors cannot all
// be honoured: tabs compress inside the bar (and their labels ellipsize at
// full font size) rather than spilling past the edge.
TEST(tab_container_narrow_bar_compresses_inside) {
  ImmTestHarness h;
  std::vector<std::string> tabs = {"Notification badge", "Dropdown morphx",
                                   "Avatar group hover", "Skeleton loader",
                                   "Streaming textxx"};
  size_t active = 0;
  tab_container(h.context(), mk(h.root(), 0), tabs, active,
                ComponentConfig{}
                    .with_size(ComponentSize{pixels(400), pixels(44)})
                    .with_debug_name("tabs"));
  h.layout_only();

  UIComponent *bar = h.find("tabs");
  CHECK(bar != nullptr);
  if (!bar)
    return;
  const float bar_right = bar->rect().x + bar->rect().width;
  for (int i = 0; i < 5; ++i) {
    UIComponent *t = h.find("tab_" + std::to_string(i));
    CHECK(t != nullptr);
    if (!t)
      return;
    fprintf(stderr, "        narrow tab_%d x=%.1f w=%.1f\n", i, t->rect().x,
            t->rect().width);
    CHECK(t->rect().width > 0.f);
    CHECK(t->rect().x >= bar->rect().x - 0.5f);
    CHECK(t->rect().x + t->rect().width <= bar_right + 0.5f);
  }
  // The squeezed space is shared in proportion to the label widths the
  // tabs wanted: equal-length labels stay equal, longer stays wider.
  UIComponent *t0 = h.find("tab_0");
  UIComponent *t1 = h.find("tab_1");
  UIComponent *t2 = h.find("tab_2");
  UIComponent *t3 = h.find("tab_3");
  UIComponent *t4 = h.find("tab_4");
  CHECK(t0 && t1 && t2 && t3 && t4);
  if (t0 && t1 && t2 && t3 && t4) {
    CHECK_APPROX(t0->rect().width, t2->rect().width); // both 18 chars
    CHECK_APPROX(t1->rect().width, t3->rect().width); // both 15 chars
    CHECK(t0->rect().width > t4->rect().width);       // 18 > 16 chars
    CHECK(t4->rect().width > t1->rect().width);       // 16 > 15 chars
  }
}

// A stale active tab clamps at entry instead of reaching the caller's switch.
TEST(tab_container_clamps_a_stale_active_tab_when_tabs_shrink) {
  ImmTestHarness h;
  std::vector<std::string> tabs = {"One", "Two", "Three"};
  size_t active = 2;
  auto emit = [&] {
    return tab_container(h.context(), mk(h.root(), 0), tabs, active,
                         ComponentConfig{}
                             .with_size(ComponentSize{pixels(600), pixels(44)})
                             .with_debug_name("tabs"));
  };
  h.begin_frame();
  emit();
  h.layout_only();
  CHECK(active == 2);

  tabs = {"Only"};
  h.begin_frame();
  auto result = emit();
  h.layout_only();
  CHECK(active == 0);
  CHECK(result.as<int>() == 0);
}

// Entity carrying the debug name (find() only returns the UIComponent).
static Entity *find_ent(ImmTestHarness &h, const std::string &name) {
  for (const auto &e : h.coll.get_entities())
    if (e && e->has<UIComponentDebug>() &&
        e->get<UIComponentDebug>().name() == name)
      return e.get();
  return nullptr;
}

// Paper variant: active tab gets the accent top edge (no bottom underline)
// and still selects on click; geometry matches the default row.
TEST(tab_container_paper_active_top_edge_and_selects) {
  ImmTestHarness h;
  std::vector<std::string> tabs = {"One", "Two", "Three"};
  size_t active = 0;
  auto emit = [&] {
    return tab_container(h.context(), mk(h.root(), 0), tabs, active,
                         ComponentConfig{}
                             .with_size(ComponentSize{pixels(600), pixels(44)})
                             .with_debug_name("tabs"),
                         TabStyle::Paper);
  };
  h.begin_frame();
  emit();
  h.layout_only();

  UIComponent *t0 = h.find("tab_0");
  UIComponent *t1 = h.find("tab_1");
  Entity *e0 = find_ent(h, "tab_0");
  Entity *e1 = find_ent(h, "tab_1");
  CHECK(t0 && t1 && e0 && e1);
  if (!t0 || !t1 || !e0 || !e1)
    return;
  auto &b0 = e0->template get<HasBorder>();
  CHECK(b0.border.top.has_border());
  CHECK(b0.border.top.thickness.value == 1.0f);
  CHECK(!b0.border.bottom.has_border());
  auto &b1 = e1->template get<HasBorder>();
  CHECK(!b1.border.top.has_border());
  // Same row as default: one bar height, tabs side by side.
  CHECK_APPROX(t0->rect().y, t1->rect().y);
  CHECK(t1->rect().x > t0->rect().x);

  // Click tab_1: press state + HandleClicks (the systems the real app runs
  // between layout and the next build), then re-emit.
  const Rectangle r = t1->rect();
  auto &ctx = h.context();
  ctx.mouse.pos = input::MousePosition{r.x + r.width * 0.5f,
                                       r.y + r.height * 0.5f};
  ctx.mouse.left_down = true;
  ctx.mouse.just_pressed = true;
  ctx.mouse.press_moved = false;
  ctx.set_hot(ctx.ROOT);
  ctx.set_active(ctx.ROOT);
  ResolveHitTarget<ui_test::TestInputAction> resolver;
  resolver.context = &ctx;
  resolver.resolve();
  HandleClicks<ui_test::TestInputAction> sys;
  sys.context = &ctx;
  for (const auto &e : h.coll.get_entities()) {
    if (!e || !e->has<UIComponent>() || !e->has<HasClickListener>())
      continue;
    sys.for_each_with(*e, e->get<UIComponent>(), e->get<HasClickListener>(),
                      0.f);
  }
  ctx.mouse.just_pressed = false;
  ctx.mouse.left_down = false;
  h.begin_frame();
  emit();
  h.layout_only();
  CHECK(active == 1);
}

// Default style unchanged: active tab has the bottom accent underline and no
// top edge.
TEST(tab_container_default_keeps_bottom_underline) {
  ImmTestHarness h;
  std::vector<std::string> tabs = {"One", "Two", "Three"};
  size_t active = 0;
  tab_container(h.context(), mk(h.root(), 0), tabs, active,
                ComponentConfig{}
                    .with_size(ComponentSize{pixels(600), pixels(44)})
                    .with_debug_name("tabs"));
  h.layout_only();

  UIComponent *t0 = h.find("tab_0");
  UIComponent *t1 = h.find("tab_1");
  Entity *e0 = find_ent(h, "tab_0");
  Entity *e1 = find_ent(h, "tab_1");
  CHECK(t0 && t1 && e0 && e1);
  if (!t0 || !t1 || !e0 || !e1)
    return;
  auto &b0 = e0->template get<HasBorder>();
  CHECK(b0.border.bottom.has_border());
  CHECK(b0.border.bottom.thickness.value == 4.0f);
  CHECK(!b0.border.top.has_border());
  auto &b1 = e1->template get<HasBorder>();
  CHECK(b1.border.bottom.has_border());
  CHECK(b1.border.bottom.thickness.value == 1.0f);
}

int main() { return ui_test::run_registered_tests("tab_container tests"); }
