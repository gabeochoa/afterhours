// e2e_clipboard_test.cpp
// E2E runs get their own clipboard, so a script's copy/paste never reads or
// overwrites the user's, and works with no window (the offscreen Metal runner
// has no sokol_app clipboard at all). set_clipboard / expect_clipboard seed
// and check it from a script.
//
// Built against raylib with no window: if anything still went to the system
// clipboard here, raylib would have nothing to write to and expect_clipboard
// would fail, so these checks cannot pass by touching the real pasteboard.
#include "ui_test_harness.h"

#include <afterhours/src/plugins/e2e_testing/runner.h>
#include <afterhours/src/plugins/ui/text_input/component.h>

#include <filesystem>
#include <fstream>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using namespace afterhours::testing;

namespace {

PendingE2ECommand run(const std::string &name, const std::string &arg) {
  PendingE2ECommand cmd;
  cmd.name = name;
  cmd.args = {arg};
  Entity dummy;
  HandleClipboardCommands{}.for_each_with(dummy, cmd, 0.f);
  return cmd;
}

} // namespace

TEST(registering_the_handlers_isolates_the_clipboard) {
  SystemManager sm;
  register_builtin_handlers(sm);
  CHECK(clipboard::detail::provider == &test_clipboard());
  clipboard::set_text("from the app");
  CHECK(test_clipboard().get_text() == "from the app");

  use_system_clipboard();
  CHECK(clipboard::detail::provider == nullptr);
  isolate_clipboard();
}

TEST(set_and_expect_clipboard) {
  isolate_clipboard();
  test_clipboard().reset();
  CHECK(run("set_clipboard", "héllo\nworld").succeeded());
  CHECK(clipboard::get_text() == "héllo\nworld");
  CHECK(run("expect_clipboard", "héllo\nworld").succeeded());

  auto miss = run("expect_clipboard", "something else");
  CHECK(!miss.succeeded());
  CHECK(miss.error_message.find("something else") != std::string::npos);
  CHECK(miss.error_message.find("héllo") != std::string::npos);
}

TEST(script_lines_parse_as_whole_text) {
  const auto path = std::filesystem::temp_directory_path() /
                    "afh_clipboard_parse.e2e";
  std::ofstream(path) << "set_clipboard two words\n"
                      << "expect_clipboard \"line one\\nline two\"\n";
  const auto cmds = parse_script(path.string());
  std::filesystem::remove(path);
  CHECK(cmds.size() == 2);
  CHECK(cmds.size() == 2 && cmds[0].args ==
                                std::vector<std::string>{"two words"});
  CHECK(cmds.size() == 2 && cmds[1].args ==
                                std::vector<std::string>{"line one\nline two"});
}

TEST(text_input_copy_and_paste_use_the_isolated_clipboard) {
  isolate_clipboard();
  test_clipboard().reset();
  ui_test::ImmTestHarness h;
  std::string value = "copied text";
  auto emit = [&]() -> Entity & {
    h.begin_frame();
    auto result = imm::text_input(
        h.context(), mk(h.root(), 0), value,
        ComponentConfig{}.with_size({pixels(300), pixels(40)}));
    h.layout_only();
    return result.ent();
  };
  Entity &entity = emit();
  h.context().set_focus(entity.id);
  emit();
  h.context().last_action = ui_test::TestInputAction::TextSelectAll;
  emit();
  h.context().last_action = ui_test::TestInputAction::TextCopy;
  emit();
  CHECK(run("expect_clipboard", "copied text").succeeded());

  run("set_clipboard", "pasted");
  h.context().last_action = ui_test::TestInputAction::TextSelectAll;
  emit();
  h.context().last_action = ui_test::TestInputAction::TextPaste;
  emit();
  CHECK(value == "pasted");
}

TEST(reset_test_state_empties_it) {
  isolate_clipboard();
  run("set_clipboard", "left over");
  PendingE2ECommand reset;
  reset.name = "reset_test_state";
  Entity dummy;
  HandleResetTestStateCommand{}.for_each_with(dummy, reset, 0.f);
  CHECK(test_clipboard().get_text().empty());
}

int main() {
  const int rc = ui_test::run_registered_tests("E2E clipboard");
  use_system_clipboard();
  return rc;
}
