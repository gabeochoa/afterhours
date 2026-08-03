// menu_test.cpp
// D13: dropdown_menu / context_menu share one anchored list, so flipping,
// separators, disabled items and dismissal behave the same in both.

#include "ui_test_harness.h"

#include <afterhours/src/plugins/ui/menu.h>

#include <string>
#include <vector>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

namespace {
std::vector<MenuItem> sample() {
  return {MenuItem{"Open", "Cmd+O", false, false},
          MenuItem{"Save", "Cmd+S", false, false},
          MenuItem::sep(),
          MenuItem{"Quit", "", false, true}};
}

int count_named(const std::string &prefix) {
  int n = 0;
  for (const auto &e : UICollectionHolder::get().collection.get_entities()) {
    if (e && e->has<UIComponentDebug>() &&
        e->get<UIComponentDebug>().name().rfind(prefix, 0) == 0)
      n++;
  }
  return n;
}
} // namespace

TEST(menu_closed_renders_nothing) {
  ImmTestHarness h;
  bool open = false;
  h.begin_frame();
  context_menu(h.context(), mk(h.root(), 0), sample(), Vector2Type{100, 100},
               open, ComponentConfig{}.with_size(
                         ComponentSize{pixels(180), pixels(28)}));
  h.layout_only();
  ui_test::check(h.find("menu_list") == nullptr, "a closed menu builds nothing",
                 __FILE__, __LINE__);
}

TEST(menu_open_renders_items_and_separator) {
  ImmTestHarness h;
  bool open = true;
  h.begin_frame();
  context_menu(h.context(), mk(h.root(), 0), sample(), Vector2Type{100, 100},
               open, ComponentConfig{}.with_size(
                         ComponentSize{pixels(180), pixels(28)}));
  h.layout_only();
  ui_test::check(h.find("menu_list") != nullptr, "the list exists", __FILE__,
                 __LINE__);
  ui_test::check(count_named("menu_item_") == 3, "three selectable items",
                 __FILE__, __LINE__);
  ui_test::check(count_named("menu_separator") == 1, "one separator", __FILE__,
                 __LINE__);
  ui_test::check(count_named("menu_shortcut") >= 2, "shortcuts rendered",
                 __FILE__, __LINE__);
}

TEST(context_menu_opens_at_the_point) {
  ImmTestHarness h;
  bool open = true;
  h.begin_frame();
  context_menu(h.context(), mk(h.root(), 0), sample(), Vector2Type{120, 90},
               open, ComponentConfig{}.with_size(
                         ComponentSize{pixels(180), pixels(28)}));
  h.layout_only();
  UIComponent *list = h.find("menu_list");
  ui_test::check(list != nullptr, "the list exists", __FILE__, __LINE__);
  if (list) {
    CHECK_APPROX(list->rect().x, 120.f);
    CHECK_APPROX(list->rect().y, 90.f);
  }
}

// The reason the shared engine exists: a menu opened near the bottom must not
// run off screen.
TEST(context_menu_flips_near_the_bottom_edge) {
  ImmTestHarness h;
  bool open = true;
  h.begin_frame();
  context_menu(h.context(), mk(h.root(), 0), sample(), Vector2Type{120, 560},
               open, ComponentConfig{}.with_size(
                         ComponentSize{pixels(180), pixels(28)}));
  h.layout_only();
  UIComponent *list = h.find("menu_list");
  ui_test::check(list != nullptr, "the list exists", __FILE__, __LINE__);
  if (list) {
    ui_test::check(list->rect().y < 560.f, "flips above the click point",
                   __FILE__, __LINE__);
    ui_test::check(list->rect().y >= 0.f, "stays on screen", __FILE__,
                   __LINE__);
  }
}

TEST(dropdown_menu_anchors_under_its_trigger) {
  ImmTestHarness h;
  bool open = true;
  h.begin_frame();
  dropdown_menu(h.context(), mk(h.root(), 0), "File", sample(), open,
                ComponentConfig{}
                    .with_size(ComponentSize{pixels(180), pixels(28)})
                    .with_absolute_position(60.f, 40.f));
  h.layout_only();
  h.begin_frame();
  dropdown_menu(h.context(), mk(h.root(), 0), "File", sample(), open,
                ComponentConfig{}
                    .with_size(ComponentSize{pixels(180), pixels(28)})
                    .with_absolute_position(60.f, 40.f));
  h.layout_only();

  UIComponent *list = h.find("menu_list");
  ui_test::check(list != nullptr, "the list exists", __FILE__, __LINE__);
  if (list)
    ui_test::check(list->rect().y > 40.f, "opens below the trigger", __FILE__,
                   __LINE__);
}

// The wm showcase case: a menu-bar trigger near the bottom edge. Only the
// downward case was covered before, which is why this went unnoticed.
TEST(dropdown_menu_flips_near_the_bottom) {
  ImmTestHarness h;
  bool open = true;
  auto emit = [&] {
    dropdown_menu(h.context(), mk(h.root(), 0), "Bottom", sample(), open,
                  ComponentConfig{}
                      .with_size(ComponentSize{pixels(150), pixels(32)})
                      .with_absolute_position(24.f, 540.f));
  };
  h.begin_frame(); emit(); h.layout_only();
  h.begin_frame(); emit(); h.layout_only();

  UIComponent *list = h.find("menu_list");
  ui_test::check(list != nullptr, "the list exists", __FILE__, __LINE__);
  if (list) {
    const float y = list->rect().y;
    ui_test::check(y < 540.f, "flips above the trigger", __FILE__, __LINE__);
    ui_test::check(y >= 0.f && y + list->rect().height <= 601.f,
                   "stays on screen", __FILE__, __LINE__);
    if (y >= 540.f)
      fprintf(stderr, "        list y=%.1f h=%.1f\n", y, list->rect().height);
  }
}

int main() { return ui_test::run_registered_tests("menu"); }
