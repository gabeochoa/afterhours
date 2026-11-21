// popup_overlay_layer_test.cpp
// R-052: popup panels live in a reserved overlay band over their anchor, so
// a text_area built after an open dropdown cannot paint its lines (+2) or
// cursor (+10) over the tray.

#include "ui_test_harness.h"

#include <afterhours/src/plugins/ui/menu.h>
#include <afterhours/src/plugins/ui/text_input/text_area.h>

#include <string>
#include <vector>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

namespace {
ComponentConfig area_config() {
  return ComponentConfig{}
      .with_size(ComponentSize{pixels(200), pixels(60)})
      .with_font(UIComponent::DEFAULT_FONT, 20.f)
      .with_line_height(pixels(20));
}

// Cursor/line layers of a focused text_area at the default layer, recorded
// before any popup opens (focusing the area would close the popup).
struct TextLayers {
  int line = -1;
  int cursor = -1;
};

TextLayers measure_text_layers(ImmTestHarness &h, std::string &text) {
  auto emit = [&] {
    text_area(h.context(), mk(h.root(), 1), text, area_config());
  };
  h.begin_frame(); emit(); h.layout_only();
  auto *field = h.find("text_area_field");
  if (field) h.context().set_focus(field->id);
  h.begin_frame(); emit(); h.layout_only();
  TextLayers out;
  if (auto *line = h.find("text_area_line")) out.line = line->render_layer;
  if (auto *cursor = h.find("text_area_cursor"))
    out.cursor = cursor->render_layer;
  return out;
}
} // namespace

TEST(dropdown_tray_clears_a_later_text_area) {
  ImmTestHarness h;
  std::string text = "hello";
  const TextLayers text_layers = measure_text_layers(h, text);
  CHECK(text_layers.line == 2);
  CHECK(text_layers.cursor == 10);

  std::vector<std::string> options{"First", "Second"};
  size_t selected = 0;
  auto emit = [&] {
    // Failing arrangement: dropdown first, text_area after it.
    dropdown(h.context(), mk(h.root(), 0), options, selected,
             ComponentConfig{}.with_size({pixels(200), pixels(40)}));
    text_area(h.context(), mk(h.root(), 1), text, area_config());
  };
  h.begin_frame(); emit(); h.layout_only();
  for (const auto &e : UICollectionHolder::get().collection.get_entities())
    if (e && e->has<HasDropdownState>()) e->get<HasDropdownState>().on = true;
  h.begin_frame(); emit(); h.layout_only();

  auto *tray = h.find("dropdown_options_tray");
  CHECK(tray != nullptr);
  if (!tray) return;
  CHECK(tray->render_layer == overlay::OVERLAY_BASE);
  CHECK(tray->render_layer > text_layers.cursor);
  CHECK(tray->render_layer > text_layers.line);
  bool tray_queued_above_cursor = false;
  for (const auto &cmd : h.context().render_cmds)
    if (cmd.id == tray->id && cmd.layer > text_layers.cursor)
      tray_queued_above_cursor = true;
  CHECK(tray_queued_above_cursor);
}

TEST(menu_and_popover_panels_use_the_overlay_band) {
  ImmTestHarness h;
  std::string text = "hello";
  const TextLayers text_layers = measure_text_layers(h, text);

  bool menu_open = true;
  bool pop_open = true;
  h.begin_frame();
  context_menu(h.context(), mk(h.root(), 0),
               std::vector<MenuItem>{MenuItem{"Open", "Cmd+O", false, false}},
               Vector2Type{300, 40}, menu_open,
               ComponentConfig{}.with_size({pixels(180), pixels(28)}));
  popover(h.context(), mk(h.root(), 2), RectangleType{300, 200, 120, 30},
          pop_open, overlay::Placement::Below,
          ComponentConfig{}.with_size({pixels(180), pixels(80)}));
  text_area(h.context(), mk(h.root(), 1), text, area_config());
  h.layout_only();

  auto *list = h.find("menu_list");
  auto *shortcut = h.find("menu_shortcut");
  auto *panel = h.find("popover_panel");
  CHECK(list != nullptr && panel != nullptr);
  if (!list || !panel) return;
  CHECK(list->render_layer == overlay::OVERLAY_BASE);
  CHECK(panel->render_layer == overlay::OVERLAY_BASE);
  CHECK(list->render_layer > text_layers.cursor);
  CHECK(panel->render_layer > text_layers.cursor);
  if (shortcut)
    CHECK(shortcut->render_layer == list->render_layer + 1);
}

TEST(nested_popups_keep_relative_order) {
  ImmTestHarness h;
  bool outer_open = true;
  bool inner_open = true;
  auto emit_outer = [&] {
    popover(h.context(), mk(h.root(), 0), RectangleType{40, 40, 120, 30},
            outer_open, overlay::Placement::Below,
            ComponentConfig{}.with_size({pixels(200), pixels(120)}));
  };
  h.begin_frame(); emit_outer(); h.layout_only();
  auto *outer = h.find("popover_panel");
  CHECK(outer != nullptr);
  if (!outer) return;
  const int outer_layer = outer->render_layer;

  h.begin_frame();
  emit_outer();
  popover(h.context(), mk(h.root(), 1), RectangleType{60, 60, 120, 30},
          inner_open, overlay::Placement::Below,
          ComponentConfig{}
              .with_size({pixels(160), pixels(80)})
              .with_render_layer(outer_layer));
  h.layout_only();

  // The inner panel has a distinct debug name only via its holder; both
  // panels share "popover_panel", so compare the two panel layers directly.
  std::vector<int> panels;
  for (const auto &e : UICollectionHolder::get().collection.get_entities())
    if (e && e->has<UIComponentDebug>() &&
        e->get<UIComponentDebug>().name() == "popover_panel")
      panels.push_back(e->get<UIComponent>().render_layer);
  CHECK(panels.size() == 2);
  if (panels.size() != 2) return;
  const int lo = panels[0] < panels[1] ? panels[0] : panels[1];
  const int hi = panels[0] < panels[1] ? panels[1] : panels[0];
  CHECK(lo == overlay::OVERLAY_BASE);
  CHECK(hi == lo + overlay::OVERLAY_BASE);
}

int main() { return ui_test::run_registered_tests("popup_overlay_layer"); }
