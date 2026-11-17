#include "ui_test_harness.h"

#include <afterhours/src/plugins/e2e_testing/runner.h>
#include <afterhours/src/plugins/e2e_testing/terminal_commands.h>

#include <cstdio>
#include <string>

using namespace afterhours;
using afterhours::testing::PendingE2ECommand;
namespace tc = afterhours::testing::terminal_commands;

static int checks_run = 0;
static int checks_passed = 0;
static void check(bool cond, const std::string &what) {
  checks_run++;
  if (cond) checks_passed++;
  else fprintf(stderr, "  FAIL: %s\n", what.c_str());
}

static Entity &make_cmd(EntityCollection &coll, const char *name,
                        std::vector<std::string> args = {}) {
  Entity &e = coll.createEntity();
  auto &cmd = e.addComponent<PendingE2ECommand>();
  cmd.name = name;
  cmd.args = std::move(args);
  return e;
}

int main() {
  terminal::Console console;
  console.add_command({"echo", "echo", [](terminal::Arguments args) {
                         return terminal::Result{args.empty() ? "" : args[0]};
                       }});
  console.execute("echo hello world");
  console.input = "echo next";
  bool open = true;

  {
    EntityCollection coll;
    Entity &e = make_cmd(coll, "expect_terminal_open");
    tc::HandleExpectTerminalOpenCommand h([&] { return open; });
    auto &cmd = e.get<PendingE2ECommand>();
    h.for_each_with(e, cmd, 0.f);
    check(cmd.is_consumed() && cmd.succeeded(), "expect_terminal_open passes when open");
  }
  {
    EntityCollection coll;
    Entity &e = make_cmd(coll, "expect_terminal_closed");
    tc::HandleExpectTerminalClosedCommand h([&] { return open; });
    auto &cmd = e.get<PendingE2ECommand>();
    h.for_each_with(e, cmd, 0.f);
    check(cmd.is_retry() && cmd.last_mismatch.find("open") != std::string::npos,
          "expect_terminal_closed retries while open");
    open = false;
    cmd.reset_retry();
    h.for_each_with(e, cmd, 0.f);
    check(cmd.is_consumed() && cmd.succeeded(), "expect_terminal_closed passes once closed");
    open = true;
  }
  {
    EntityCollection coll;
    Entity &e = make_cmd(coll, "expect_terminal_input", {"echo next"});
    tc::HandleExpectTerminalInputCommand h(console);
    auto &cmd = e.get<PendingE2ECommand>();
    h.for_each_with(e, cmd, 0.f);
    check(cmd.is_consumed() && cmd.succeeded(), "expect_terminal_input matches console.input");
  }
  {
    EntityCollection coll;
    Entity &e = make_cmd(coll, "expect_terminal_input", {"other"});
    tc::HandleExpectTerminalInputCommand h(console);
    auto &cmd = e.get<PendingE2ECommand>();
    h.for_each_with(e, cmd, 0.f);
    check(cmd.is_retry() && cmd.last_mismatch.find("echo next") != std::string::npos,
          "expect_terminal_input mismatch names actual input");
  }
  {
    EntityCollection coll;
    Entity &e = make_cmd(coll, "expect_terminal_output_contains", {"hello world"});
    tc::HandleExpectTerminalOutputContainsCommand h(console);
    auto &cmd = e.get<PendingE2ECommand>();
    h.for_each_with(e, cmd, 0.f);
    check(cmd.is_consumed() && cmd.succeeded(), "expect_terminal_output_contains finds a line");
  }
  {
    EntityCollection coll;
    Entity &e = make_cmd(coll, "expect_terminal_output_contains", {"missing text"});
    tc::HandleExpectTerminalOutputContainsCommand h(console);
    auto &cmd = e.get<PendingE2ECommand>();
    h.for_each_with(e, cmd, 0.f);
    check(cmd.is_retry() && cmd.last_mismatch.find("hello world") != std::string::npos,
          "expect_terminal_output_contains mismatch shows output");
  }
  {
    EntityCollection coll;
    Entity &e = make_cmd(coll, "expect_terminal_input");
    tc::HandleExpectTerminalInputCommand h(console);
    auto &cmd = e.get<PendingE2ECommand>();
    h.for_each_with(e, cmd, 0.f);
    check(cmd.is_consumed() && !cmd.succeeded(), "expect_terminal_input without text fails");
  }
  {
    const char *path = "/tmp/afterhours_terminal_commands_test.e2e";
    FILE *f = fopen(path, "w");
    fputs("expect_terminal_open\n"
          "expect_terminal_input echo next\n"
          "expect_terminal_output_contains hello world\n",
          f);
    fclose(f);
    auto cmds = testing::parse_script(path);
    check(cmds.size() == 3, "terminal script parses three commands");
    if (cmds.size() == 3) {
      check(cmds[1].args.size() == 1 && cmds[1].args[0] == "echo next",
            "expect_terminal_input keeps rest of line as one arg");
      check(cmds[2].args.size() == 1 && cmds[2].args[0] == "hello world",
            "expect_terminal_output_contains keeps rest of line as one arg");
    }
    remove(path);
  }

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) return 1;
  printf("All checks passed!\n");
  return 0;
}
