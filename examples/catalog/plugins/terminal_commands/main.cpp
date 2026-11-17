#define AFTER_HOURS_USE_NONE 1
#define FMT_HEADER_ONLY 1
#include "../../../../src/plugins/e2e_testing/terminal_commands.h"

#include <cassert>
#include <cstdio>

using namespace afterhours;
namespace tc = testing::terminal_commands;

int main() {
  terminal::Console console;
  console.add_command({"echo", "echo text", [](terminal::Arguments args) {
                         return terminal::Result{args.empty() ? "" : args[0]};
                       }});
  console.execute("echo hello world");
  console.input = "echo next";
  bool open = true;

  SystemManager systems;
  tc::register_terminal_commands(systems, console, [&] { return open; });

  auto run = [](auto &handler, const char *name, std::vector<std::string> args = {}) {
    EntityCollection coll;
    Entity &e = coll.createEntity();
    auto &cmd = e.addComponent<testing::PendingE2ECommand>();
    cmd.name = name;
    cmd.args = std::move(args);
    handler.for_each_with(e, cmd, 0.f);
    return cmd.is_consumed() && cmd.succeeded();
  };

  tc::HandleExpectTerminalOpenCommand expect_open([&] { return open; });
  assert(run(expect_open, "expect_terminal_open"));
  tc::HandleExpectTerminalInputCommand expect_input(console);
  assert(run(expect_input, "expect_terminal_input", {"echo next"}));
  tc::HandleExpectTerminalOutputContainsCommand expect_output(console);
  assert(run(expect_output, "expect_terminal_output_contains", {"hello world"}));
  open = false;
  tc::HandleExpectTerminalClosedCommand expect_closed([&] { return open; });
  assert(run(expect_closed, "expect_terminal_closed"));

  std::puts("terminal_commands: open, input and output assertions passed");
  return 0;
}
