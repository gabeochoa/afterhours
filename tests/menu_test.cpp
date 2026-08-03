// menu_test.cpp
// D13: dropdown_menu / context_menu share one anchored list, so flipping,
// separators, disabled items and dismissal behave the same in both.

#include "ui_test_harness.h"

#include <afterhours/src/plugins/ui/menu.h>

#include <cmath>
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

// Screenshot bug: every menu sat exactly one trigger-height too low -- the
// bottom-anchored menu drew its last item over its own trigger, and the top
// ones left a 32px gap under theirs.
//
// The list's absolute position is relative to its immediate parent, which is
// the zero-size menu_root, but the screen->local conversion was subtracting
// menu_root's PARENT (the dropdown holder). menu_root flows after the trigger
// inside that holder, so the difference was the trigger's height every time.
//
// "y > trigger" was the old assertion and it passed throughout. Flush is the
// property that actually pins this down.
TEST(dropdown_menu_list_sits_flush_under_the_trigger) {
  ImmTestHarness h;
  bool open = true;
  auto emit = [&] {
    dropdown_menu(h.context(), mk(h.root(), 0), "File", sample(), open,
                  ComponentConfig{}
                      .with_size(ComponentSize{pixels(180), pixels(28)})
                      .with_absolute_position(60.f, 40.f));
  };
  // Two frames: the conversion reads rects laid out by the previous one.
  h.begin_frame(); emit(); h.layout_only();
  h.begin_frame(); emit(); h.layout_only();

  UIComponent *holder = h.find("dropdown_menu");
  UIComponent *list = h.find("menu_list");
  ui_test::check(holder != nullptr && list != nullptr, "both exist", __FILE__,
                 __LINE__);
  if (!holder || !list)
    return;
  const float trigger_bottom = holder->rect().y + holder->rect().height;
  ui_test::check(std::abs(list->rect().y - trigger_bottom) < 0.5f,
                 "the list touches the bottom of the trigger", __FILE__,
                 __LINE__);
  ui_test::check(std::abs(list->rect().x - holder->rect().x) < 0.5f,
                 "the list shares the trigger's left edge", __FILE__, __LINE__);
  if (std::abs(list->rect().y - trigger_bottom) >= 0.5f)
    fprintf(stderr, "        list y=%.1f trigger bottom=%.1f\n", list->rect().y,
            trigger_bottom);
}

// The flipped mirror of the above: the menu's bottom edge lands on the top of
// the trigger, and in particular does NOT overlap it.
TEST(flipped_dropdown_sits_flush_above_the_trigger) {
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

  UIComponent *holder = h.find("dropdown_menu");
  UIComponent *list = h.find("menu_list");
  ui_test::check(holder != nullptr && list != nullptr, "both exist", __FILE__,
                 __LINE__);
  if (!holder || !list)
    return;
  const float list_bottom = list->rect().y + list->rect().height;
  ui_test::check(list_bottom <= holder->rect().y + 0.5f,
                 "the list does not overlap its own trigger", __FILE__,
                 __LINE__);
  if (list_bottom > holder->rect().y + 0.5f)
    fprintf(stderr, "        list bottom=%.1f trigger top=%.1f\n", list_bottom,
            holder->rect().y);
}

// Screenshot bug: the item label defaults to centre alignment across the full
// row, so with a shortcut sitting in the right-hand gutter the two run
// together -- "New" and "Cmd+N" rendered as "NewCmd+N". The label has to be
// left-aligned so it starts at the row's left edge and leaves the gutter free.
//
// Asserting on the drawn text positions rather than on the component rects,
// because the rects were already correct when this shipped broken: the item
// spans the whole row by design and it is only the *text* inside it that moved.
TEST(menu_label_starts_at_the_left_edge) {
  ImmTestHarness h;
  bool open = true;
  std::vector<MenuItem> items{MenuItem{"Open", "Cmd+O", false, false}};
  h.begin_frame();
  context_menu(h.context(), mk(h.root(), 0), items, Vector2Type{40, 40}, open,
               ComponentConfig{}.with_size(
                   ComponentSize{pixels(200), pixels(28)}));
  h.render();

  UIComponent *item = h.find("menu_item_0");
  ui_test::check(item != nullptr, "the item exists", __FILE__, __LINE__);
  if (!item)
    return;

  float label_x = -1.f, shortcut_x = -1.f;
  for (const auto &c : h.drawn("text")) {
    if (c.text == "Open")
      label_x = c.rect.x;
    if (c.text == "Cmd+O")
      shortcut_x = c.rect.x;
  }
  ui_test::check(label_x >= 0.f, "the label was drawn", __FILE__, __LINE__);
  ui_test::check(shortcut_x >= 0.f, "the shortcut was drawn", __FILE__,
                 __LINE__);
  if (label_x < 0.f || shortcut_x < 0.f)
    return;

  // Centred in a 200px row, "Open" would land near x+80. Left-aligned it sits
  // within the text margin of the edge, so a quarter of the width is a wide
  // margin that still separates the two cases cleanly.
  const float left_limit = item->rect().x + item->rect().width * 0.25f;
  ui_test::check(label_x < left_limit, "the label is left-aligned", __FILE__,
                 __LINE__);
  ui_test::check(label_x < shortcut_x, "the label starts before the shortcut",
                 __FILE__, __LINE__);
  if (label_x >= left_limit)
    fprintf(stderr, "        label x=%.1f limit=%.1f shortcut x=%.1f\n",
            label_x, left_limit, shortcut_x);
}

namespace {
// Width of the shortcut gutter for a menu whose only item has this shortcut.
// Each harness clears the UI collection on construction, so these do not see
// each other's entities as long as they are built one at a time.
float gutter_width_for(const std::string &shortcut) {
  ImmTestHarness h;
  bool open = true;
  std::vector<MenuItem> items{MenuItem{"Open", shortcut, false, false}};
  h.begin_frame();
  context_menu(h.context(), mk(h.root(), 0), items, Vector2Type{40, 40}, open,
               ComponentConfig{}.with_size(
                   ComponentSize{pixels(200), pixels(28)}));
  h.layout_only();
  UIComponent *sc = h.find("menu_shortcut");
  return sc ? sc->rect().width : -1.f;
}
} // namespace

// The other half of the same screenshot: the gutter was a fixed fraction of the
// menu width, so a long shortcut overflowed it and drew over the next column.
// It has to scale with the content it holds.
TEST(menu_gutter_grows_with_the_longest_shortcut) {
  const float narrow = gutter_width_for("Cmd+S");
  const float wide = gutter_width_for("Shift+Cmd+Z");
  ui_test::check(narrow > 0.f && wide > 0.f, "both gutters were built",
                 __FILE__, __LINE__);
  ui_test::check(wide > narrow, "a longer shortcut gets a wider gutter",
                 __FILE__, __LINE__);
  // Half the menu is the ceiling -- past that the label has nowhere to go, and
  // truncating the shortcut is the better failure.
  ui_test::check(wide <= 100.f, "the gutter never takes more than half",
                 __FILE__, __LINE__);
  if (wide <= narrow)
    fprintf(stderr, "        narrow=%.1f wide=%.1f\n", narrow, wide);
}

namespace {
// True when the named element carries rounded corners.
bool has_rounding(const std::string &name) {
  for (const auto &e : UICollectionHolder::get().collection.get_entities()) {
    if (e && e->has<UIComponentDebug>() &&
        e->get<UIComponentDebug>().name() == name)
      return e->has<HasRoundedCorners>();
  }
  return false;
}
} // namespace

// The theme rounds all four corners by default, so every part of a menu
// inherited it: rounded rows notched into each other, and the rounded panel
// showed through the last row wherever that row was disabled -- disabled
// backgrounds are translucent (Theme::disabled_variant scales alpha), so the
// panel's curved corner was visible straight through it.
//
// Nothing inside a menu should round. with_roundness(0) is NOT enough: it is
// only read when rounded_corners is also set, so the theme's corners survive
// and the element stays rounded.
TEST(menu_parts_have_no_rounded_corners) {
  ImmTestHarness h;
  bool open = true;
  auto emit = [&] {
    dropdown_menu(h.context(), mk(h.root(), 0), "File", sample(), open,
                  ComponentConfig{}
                      .with_size(ComponentSize{pixels(180), pixels(28)})
                      .with_absolute_position(60.f, 40.f));
  };
  h.begin_frame(); emit(); h.layout_only();
  h.begin_frame(); emit(); h.layout_only();

  for (const char *name : {"menu_list", "menu_item_0", "menu_separator",
                           "dropdown_menu_trigger"}) {
    ui_test::check(!has_rounding(name),
                   (std::string(name) + " has square corners").c_str(),
                   __FILE__, __LINE__);
  }
}

int main() { return ui_test::run_registered_tests("menu"); }
