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

int main() { return ui_test::run_registered_tests("terminal UI"); }
