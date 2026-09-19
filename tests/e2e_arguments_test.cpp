#include "ui_test_harness.h"
#include <afterhours/src/plugins/e2e_testing/runner.h>

#include <chrono>
#include <fstream>

using namespace afterhours;
using namespace afterhours::testing;

struct ScriptFile {
  std::filesystem::path directory = std::filesystem::temp_directory_path() /
      ("afterhours-arguments-" + std::to_string(
          std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::path path = directory / "arguments.e2e";

  explicit ScriptFile(std::string_view text) {
    std::filesystem::create_directory(directory);
    std::ofstream(path) << text;
  }
  ~ScriptFile() { std::filesystem::remove_all(directory); }
};

static void check_args(std::string_view line, std::vector<std::string> expected) {
  ScriptFile file(line);
  auto commands = parse_script(file.path.string());
  CHECK(commands.size() == 1);
  if (commands.size() != 1) return;
  CHECK(commands[0].args == expected);
  CHECK(commands[0].parse_error.empty());
}

TEST(quoted_properties_and_custom_arguments) {
  check_args(R"e2e(assert_ui file_header_label "text=a-small.cpp  +1  (new file)" x=10)e2e",
             {"file_header_label", "text=a-small.cpp  +1  (new file)", "x=10"});
  check_args(R"e2e(assert_ui "file header" text="a-small.cpp  +1  (new file)" hidden=false)e2e",
             {"file header", "text=a-small.cpp  +1  (new file)", "hidden=false"});
  check_args(R"e2e(custom "" text="" prefix"two words"suffix)e2e",
             {"", "text=", "prefixtwo wordssuffix"});
  check_args(R"e2e(custom "say \"hello\"" "C:\\Users\\test" "literal\n" C:\Users\test don't)e2e",
             {"say \"hello\"", "C:\\Users\\test", "literal\\n", "C:\\Users\\test", "don't"});
}

TEST(builtins_share_quoted_arguments) {
  check_args(R"e2e(assert_ui_text "say \"hello\"" x=10 "text=say \"hello\"")e2e",
             {"say \"hello\"", "x=10", "text=say \"hello\""});
  check_args(R"e2e(select_dropdown "menu name" "item name")e2e", {"menu name", "item name"});
  check_args(R"e2e(double_click_ui "field name" 10 20)e2e", {"field name", "10", "20"});
  check_args(R"e2e(expect_input_selection "field name" 0 5)e2e", {"field name", "0", "5"});
  check_args(R"e2e(expect_input_text "field name" "a \"quote\" and \\ path")e2e",
             {"field name", "a \"quote\" and \\ path"});
}

TEST(free_text_stays_literal_unless_quoted) {
  check_args(R"e2e(type echo  "hello" C:\new\test)e2e", {R"e2e(echo  "hello" C:\new\test)e2e"});
  check_args(R"e2e(type "echo  \"hello\" C:\\new\\test")e2e", {R"e2e(echo  "hello" C:\new\test)e2e"});
  check_args("expect_text plain  text", {"plain  text"});
  check_args("expect_input_text field plain  text", {"field", "plain  text"});
  check_args(R"e2e(type "")e2e", {""});
  check_args("click_text New  project", {"New  project"});
  check_args("custom value\r\n", {"value"});
}

TEST(quoted_property_reaches_the_assertion_handler) {
  ui_test::ImmTestHarness h;
  h.begin_frame();
  ui::imm::div(h.context(), ui::imm::mk(h.root(), 0), ui::imm::ComponentConfig{}
      .with_size({ui::pixels(300), ui::pixels(40)})
      .with_label("a-small.cpp  +1  (new file)").with_debug_name("file_header_label"));
  h.layout_only();
  ScriptFile file(R"e2e(assert_ui file_header_label "text=a-small.cpp  +1  (new file)" hidden=false)e2e");
  auto parsed = parse_script(file.path.string()).front();
  PendingE2ECommand pending;
  pending.name = parsed.name;
  pending.args = parsed.args;
  ui_commands::HandleAssertUICommand{}.for_each_with(h.root(), pending, 0.f);
  CHECK(pending.is_consumed());
  CHECK(pending.error_message.empty());
}

TEST(validation_keeps_free_text_and_decodes_quoted_values) {
  check_args(R"e2e(validate title=plain  text)e2e", {"title", "plain  text"});
  check_args(R"e2e(validate title="quoted  text")e2e", {"title", "quoted  text"});
  check_args(R"e2e(validate "title=quoted  text")e2e", {"title", "quoted  text"});
  check_args(R"e2e(validate title="a \"quote\" and \\ path")e2e", {"title", "a \"quote\" and \\ path"});
  check_args(R"e2e(click_text "")e2e", {""});
}

TEST(malformed_quotes_fail_with_line_numbers_and_never_dispatch) {
  for (const auto line : {"custom text=\"unfinished", "type \"unfinished",
                          "expect_text \"closed\" unexpected", "type \"trailing\\",
                          "validate title=\"unfinished"}) {
    ScriptFile file(std::string("# heading\n\n") + line + "\n");
    const auto commands = parse_script(file.path.string());
    CHECK(commands.size() == 1);
    if (commands.size() != 1) continue;
    CHECK(!commands.front().parse_error.empty());
    CHECK(commands.front().line_number == 3);
    CHECK(commands.front().args.empty());
    ui_test::ImmTestHarness h;
    E2ERunner runner;
    runner.load_script(file.path.string());
    for (int frame = 0; frame < 20 && !runner.is_finished(); ++frame) {
      runner.tick(0.1f);
      h.coll.merge_entity_arrays();
    }
    CHECK(runner.has_failed());
    CHECK(runner.is_finished());
    CHECK(!runner.has_timed_out());
    CHECK(EntityQuery().whereHasComponent<PendingE2ECommand>().gen_count() == 0);
  }
}

TEST(malformed_script_does_not_fail_the_next_batch_script) {
  static std::string captured;
  captured.clear();
  const auto previous_sink = log_sink_fn;
  log_sink_fn = [](const char *, const char *message) { captured += message; };
  ScriptFile file("custom \"unfinished\n");
  std::ofstream(file.directory / "next.e2e") << "wait_frames 1\n";
  ui_test::ImmTestHarness h;
  E2ERunner runner;
  runner.load_scripts_from_directory(file.directory.string());
  E2ECommandCleanupSystem cleanup;
  for (int frame = 0; frame < 80 && !runner.is_finished(); ++frame) {
    runner.tick(0.1f);
    h.coll.merge_entity_arrays();
    for (auto &e : h.coll.get_entities()) {
      if (!e || !e->has<PendingE2ECommand>()) continue;
      auto &command = e->get<PendingE2ECommand>();
      if (command.name == "wait_frames") command.consume();
      cleanup.for_each_with(*e, command, 0.f);
    }
  }
  CHECK(runner.has_failed());
  CHECK(runner.is_finished());
  CHECK(!runner.has_timed_out());
  log_sink_fn = previous_sink;
  CHECK(captured.find("[FAIL] arguments") != std::string::npos);
  CHECK(captured.find("[PASS] next") != std::string::npos);
}

int main() { return ui_test::run_registered_tests("E2E arguments"); }
