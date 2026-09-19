#include "ui_test_harness.h"
#include <afterhours/src/plugins/command_picker/command_picker.h>

TEST(picker_keyboard_search_availability_and_focus) {
  using namespace afterhours;
  ui_test::ImmTestHarness h;
  terminal::Console commands;
  bool ready = false;
  int calls = 0;
  commands.add_command({"save", "Save the document", [&](terminal::Arguments) {
    ++calls;
    return terminal::Result{"Saved"};
  }, {}, {}, "save [path]", [&]() -> std::optional<std::string> {
    if (!ready) return "Open a document first";
    return {};
  }});
  command_picker::Picker picker{{{"save", "Save document", "File", "Ctrl+S"}}};
  command_picker::Style style;
  style.selected_row.with_custom_background({50, 60, 70, 255});
  auto frame = [&] {
    h.begin_frame();
    command_picker::panel(h.context(), ui::imm::mk(h.root(), 0), commands, picker, {}, style);
    h.layout_only();
    auto *list = h.find("command_picker_list");
    if (!list) return;
    auto &scroll = ui::UICollectionHolder::getEntityForID(list->id).asE().get<ui::HasScrollView>();
    scroll.viewport_size = {list->rect().width, list->rect().height};
    scroll.content_size = ui::measure_scroll_content(*list, scroll);
  };
  frame();
  CHECK(h.find("command_picker_search") != nullptr);
  CHECK(h.find("command_picker_row_0") != nullptr);
  auto *selected = h.find("command_picker_row_0");
  CHECK(selected && ui::UICollectionHolder::getEntityForID(selected->id).asE().get<HasColor>().color().r == 50);
  auto *field = h.find("command_picker_search");
  CHECK(field && h.context().focus_in_subtree(field->id));
  picker.query = "sv";
  frame();
  CHECK(picker.count() == 1);
  h.context().last_action = ui_test::TestInputAction::WidgetPress;
  frame();
  CHECK(calls == 0);
  CHECK(picker.result.text == "Unavailable: Open a document first");
  ready = true;
  h.context().last_action = ui_test::TestInputAction::WidgetPress;
  frame();
  CHECK(calls == 1 && picker.result.text == "Saved");
  h.context().set_focus(h.context().FAKE);
  h.context().last_action = ui_test::TestInputAction::WidgetPress;
  frame();
  CHECK(calls == 1);
}

TEST(picker_reveals_last_row_and_accepts_pointer) {
  using namespace afterhours;
  ui_test::ImmTestHarness h;
  h.context().scaling_mode = ui::ScalingMode::Adaptive;
  h.context().theme.ui_scale = 1.5f;
  terminal::Console commands;
  int calls = 0;
  commands.add_command({"run", "Run", [&](terminal::Arguments) {
    ++calls;
    return terminal::Result{"Done"};
  }});
  std::vector<command_picker::Entry> entries;
  for (int i = 0; i < 200; ++i)
    entries.push_back({"run", "Entry " + std::to_string(i), "Tests", ""});
  command_picker::Picker picker(std::move(entries));
  auto frame = [&] {
    h.begin_frame();
    command_picker::panel(h.context(), ui::imm::mk(h.root(), 0), commands, picker,
        ui::imm::ComponentConfig{}.with_size({ui::pixels(400.f), ui::pixels(400.f)}));
    h.layout_only();
    std::vector<Entity *> mapping;
    for (const auto &entity : h.coll.get_entities()) {
      if (!entity) continue;
      mapping.resize(std::max(mapping.size(), static_cast<size_t>(entity->id) + 1));
      mapping[static_cast<size_t>(entity->id)] = entity.get();
    }
    ui::AutoLayout::autolayout(h.root().get<ui::UIComponent>(), {800, 600}, mapping,
                               false, h.context().theme.ui_scale);
    auto *list = h.find("command_picker_list");
    if (!list) return;
    auto &scroll = ui::UICollectionHolder::getEntityForID(list->id).asE().get<ui::HasScrollView>();
    scroll.viewport_size = {list->rect().width, list->rect().height};
    scroll.content_size = ui::measure_scroll_content(*list, scroll);
  };
  frame();
  frame();
  h.context().last_action = ui_test::TestInputAction::WidgetUp;
  frame();
  frame();
  CHECK(picker.selected() == 199);
  auto *list = h.find("command_picker_list");
  CHECK(list != nullptr);
  if (!list) return;
  auto list_entity = ui::UICollectionHolder::getEntityForID(list->id);
  CHECK(list_entity.asE().get<ui::HasScrollView>().scroll_offset.y > 0.f);
  auto *last = h.find("command_picker_row_199");
  CHECK(last != nullptr);
  if (!last) return;
  const auto offset = list_entity.asE().get<ui::HasScrollView>().scroll_offset.y;
  CHECK(last->rect().y - offset >= list->rect().y - 0.5f);
  CHECK(last->rect().y + last->rect().height - offset <= list->rect().y + list->rect().height + 0.5f);
  ui::UICollectionHolder::getEntityForID(last->id).asE().get<ui::HasClickListener>().down = true;
  frame();
  CHECK(calls == 1);
  picker.query = "not found";
  frame();
  CHECK(picker.count() == 0);
  h.context().last_action = ui_test::TestInputAction::MenuBack;
  frame();
  CHECK(picker.query.empty() && picker.count() == 200);
}

int main() { return ui_test::run_registered_tests("command picker UI"); }
