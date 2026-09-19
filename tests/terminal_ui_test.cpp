#include "ui_test_harness.h"
#include <afterhours/src/plugins/terminal/terminal.h>

TEST(terminal_panel_builds_native_controls) {
  ui_test::ImmTestHarness h;
  afterhours::terminal::Console console;
  console.execute("help");
  afterhours::terminal::panel(h.context(), afterhours::ui::imm::mk(h.root(), 0),
                              console);
  h.layout_only();
  CHECK(h.find("terminal_input") != nullptr);
  CHECK(h.find("terminal_run") != nullptr);
  CHECK(h.find("terminal_output") != nullptr);
  CHECK(h.context().focus_id >= 0);
}

TEST(terminal_submission_requires_input_focus) {
  ui_test::ImmTestHarness h;
  afterhours::terminal::Console console;
  int calls = 0;
  console.add_command({"count", "", [&](afterhours::terminal::Arguments) {
    ++calls;
    return afterhours::terminal::Result{"done"};
  }});
  auto frame = [&] {
    h.begin_frame();
    afterhours::terminal::panel(h.context(), afterhours::ui::imm::mk(h.root(), 0),
                                console);
    h.layout_only();
  };
  frame();
  console.input = "count";
  h.context().last_action = ui_test::TestInputAction::WidgetPress;
  frame();
  CHECK(calls == 1);
  CHECK(console.input.empty());
  console.input = "count";
  h.context().set_focus(h.context().FAKE);
  h.context().last_action = ui_test::TestInputAction::WidgetPress;
  frame();
  CHECK(calls == 1);
  CHECK(console.input == "count");
}

TEST(terminal_panel_queues_until_app_drains) {
  ui_test::ImmTestHarness h;
  afterhours::terminal::Console console;
  console.execution = afterhours::terminal::Execution::Queued;
  int calls = 0;
  console.add_command({"count", "", [&](afterhours::terminal::Arguments) {
    ++calls;
    return afterhours::terminal::Result{"done"};
  }});
  auto frame = [&] {
    h.begin_frame();
    afterhours::terminal::panel(h.context(), afterhours::ui::imm::mk(h.root(), 0), console);
    h.layout_only();
  };
  frame();
  console.input = "count";
  h.context().last_action = ui_test::TestInputAction::WidgetPress;
  frame();
  CHECK(calls == 0);
  CHECK(console.input.empty());
  CHECK(console.pending_count() == 1);
  CHECK(console.output().back().text == "> count");
  console.drain();
  CHECK(calls == 1);
  CHECK(console.output().back().text == "done");
  frame();
  CHECK(h.find("terminal_output") != nullptr);
}

TEST(terminal_autocomplete_keyboard_and_dismissal) {
  ui_test::ImmTestHarness h;
  afterhours::terminal::Console console;
  console.enter_accepts_first_suggestion = false;
  int calls = 0;
  console.add_command({"count", "", [&](afterhours::terminal::Arguments) {
    ++calls;
    return afterhours::terminal::Result{};
  }});
  auto frame = [&] {
    h.begin_frame();
    afterhours::terminal::panel(h.context(), afterhours::ui::imm::mk(h.root(), 0), console);
    h.layout_only();
  };
  frame();
  console.input = "c";
  frame();
  CHECK(h.find("terminal_suggestions") != nullptr);
  CHECK(h.find("terminal_suggestion_1") != nullptr);
  h.context().last_action = ui_test::TestInputAction::WidgetDown;
  frame();
  h.context().last_action = ui_test::TestInputAction::WidgetPress;
  frame();
  CHECK(console.input == "count ");
  CHECK(calls == 0);
  h.context().last_action = ui_test::TestInputAction::WidgetPress;
  frame();
  CHECK(calls == 1);
  console.input = "c";
  frame();
  const auto focus = h.context().focus_id;
  h.context().last_action = ui_test::TestInputAction::MenuBack;
  frame();
  CHECK(h.context().focus_id == focus);
  h.context().last_action = ui_test::TestInputAction::WidgetUp;
  frame();
  CHECK(console.input == "count ");
  console.input = "co";
  frame();
  h.context().last_action = ui_test::TestInputAction::WidgetNext;
  frame();
  CHECK(console.input == "count ");
  CHECK(calls == 1);
}

TEST(terminal_enter_submits_unless_a_suggestion_was_selected) {
  ui_test::ImmTestHarness h;
  afterhours::terminal::Console console;
  console.enter_accepts_first_suggestion = false;
  console.execution = afterhours::terminal::Execution::Queued;
  auto frame = [&] {
    h.begin_frame();
    afterhours::terminal::panel(h.context(), afterhours::ui::imm::mk(h.root(), 0), console);
    h.layout_only();
  };
  auto press = [&](ui_test::TestInputAction action) {
    h.context().last_action = action;
    frame();
  };
  frame();
  console.input = "help ";
  frame();
  CHECK(h.find("terminal_suggestions") != nullptr);
  press(ui_test::TestInputAction::WidgetPress);
  CHECK(console.input.empty());
  CHECK(console.history().back() == "help ");
  CHECK(console.pending_count() == 1);
  console.drain();
  CHECK(console.output().back().text == "help [command] - List commands or show help");

  console.input = "h";
  frame();
  press(ui_test::TestInputAction::WidgetUp);
  press(ui_test::TestInputAction::WidgetPress);
  CHECK(console.input == "help ");
  CHECK(console.pending_count() == 0);
  press(ui_test::TestInputAction::WidgetPress);
  CHECK(console.input.empty());
  CHECK(console.pending_count() == 1);
  console.drain();

  console.input = "help ";
  frame();
  press(ui_test::TestInputAction::WidgetDown);
  console.input = "help";
  frame();
  console.input = "help ";
  frame();
  press(ui_test::TestInputAction::WidgetPress);
  CHECK(console.input.empty());
  CHECK(console.history().back() == "help ");
  console.drain();

  console.input = "h";
  frame();
  press(ui_test::TestInputAction::WidgetNext);
  CHECK(console.input == "help ");
  press(ui_test::TestInputAction::WidgetPress);
  CHECK(console.input.empty());
  console.drain();

  console.enter_accepts_first_suggestion = true;
  console.input = "help ";
  frame();
  press(ui_test::TestInputAction::WidgetPress);
  CHECK(console.input == "help clear ");
  CHECK(console.pending_count() == 0);
}

TEST(terminal_autocomplete_accepts_pointer_after_input_blurs) {
  ui_test::ImmTestHarness h;
  afterhours::terminal::Console console;
  console.enter_accepts_first_suggestion = false;
  auto frame = [&] {
    h.begin_frame();
    afterhours::terminal::panel(h.context(), afterhours::ui::imm::mk(h.root(), 0), console);
    h.layout_only();
  };
  frame();
  console.input = "he";
  frame();
  auto *suggestion = h.find("terminal_suggestion_0");
  CHECK(suggestion != nullptr);
  if (!suggestion) return;
  auto entity = afterhours::ui::UICollectionHolder::getEntityForID(suggestion->id);
  CHECK(entity.has_value());
  if (!entity) return;
  h.context().set_focus(h.context().FAKE);
  h.context().prev_hot_id = suggestion->id;
  entity.asE().get<afterhours::ui::HasClickListener>().down = true;
  frame();
  CHECK(console.input == "help ");
  CHECK(console.output().empty());
  CHECK(h.context().focus_id != h.context().FAKE);
}

TEST(terminal_autocomplete_style_is_local_and_resets) {
  using namespace afterhours;
  using namespace afterhours::ui;
  using namespace afterhours::ui::imm;
  ui_test::ImmTestHarness h;
  terminal::Console console;
  console.input = "h";
  const auto accent = h.context().theme.accent;
  terminal::AutocompleteStyle style;
  style.list.with_size({pixels(320.f), children()}).with_corner_radius(11.f);
  style.row.with_size({percent(1.f), pixels(36.f)}).with_corner_radius(3.f);
  style.selected_row.with_custom_background({55, 65, 75, 255})
      .with_custom_text_color({210, 70, 80, 255});
  auto frame = [&](const terminal::AutocompleteStyle &overrides) {
    h.begin_frame();
    terminal::panel(h.context(), mk(h.root(), 0), console, {}, overrides);
    h.layout_only();
  };
  frame(style);
  auto *list = h.find("terminal_suggestions");
  auto *row = h.find("terminal_suggestion_0");
  auto *name = h.find("terminal_suggestion_name_0");
  auto *description = h.find("terminal_suggestion_description_0");
  CHECK(list && row && name && description);
  if (!list || !row || !name || !description) return;
  CHECK_APPROX(list->rect().width, 320.f);
  CHECK_APPROX(row->rect().height, 36.f);
  CHECK(name->rect().x + name->rect().width <= description->rect().x + 0.1f);
  auto row_entity = UICollectionHolder::getEntityForID(row->id);
  CHECK(row_entity.asE().get<HasColor>().color().r == 55);
  auto name_entity = UICollectionHolder::getEntityForID(name->id);
  CHECK(name_entity.asE().get<HasLabel>().explicit_text_color.has_value());
  CHECK(name_entity.asE().get<HasLabel>().explicit_text_color->r == 210);
  CHECK(name_entity.asE().get<HasLabel>().explicit_text_color->g == 70);
  CHECK(row_entity.asE().get<HasRoundedCorners>().radius_px.value() == 3.f);
  CHECK(h.context().theme.accent.r == accent.r);
  CHECK(h.context().theme.accent.g == accent.g);
  CHECK(h.context().theme.accent.b == accent.b);
  frame({});
  CHECK(row_entity.asE().get<HasColor>().color().a == 0);
  CHECK(row_entity.asE().get<HasRoundedCorners>().radius_px.value() == 0.f);
}

int main() { return ui_test::run_registered_tests("terminal UI"); }
