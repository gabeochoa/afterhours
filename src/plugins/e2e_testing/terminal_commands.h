#pragma once

#include <format>
#include <functional>
#include <string>
#include <utility>

#include "../terminal/console.h"
#include "pending_command.h"

namespace afterhours {
namespace testing {
namespace terminal_commands {

inline std::string join_args(const PendingE2ECommand &cmd) {
  std::string out;
  for (size_t i = 0; i < cmd.args.size(); ++i) {
    if (i) out += ' ';
    out += cmd.args[i];
  }
  return out;
}

struct HandleExpectTerminalOpenCommand : System<PendingE2ECommand> {
  std::function<bool()> is_open;
  explicit HandleExpectTerminalOpenCommand(std::function<bool()> fn)
      : is_open(std::move(fn)) {}
  void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("expect_terminal_open")) return;
    if (!is_open) {
      cmd.fail("expect_terminal_open unavailable: no open-state provider configured");
      return;
    }
    if (is_open()) {
      cmd.consume();
      return;
    }
    cmd.retry_with("terminal is closed, expected open");
  }
};

struct HandleExpectTerminalClosedCommand : System<PendingE2ECommand> {
  std::function<bool()> is_open;
  explicit HandleExpectTerminalClosedCommand(std::function<bool()> fn)
      : is_open(std::move(fn)) {}
  void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("expect_terminal_closed")) return;
    if (!is_open) {
      cmd.fail("expect_terminal_closed unavailable: no open-state provider configured");
      return;
    }
    if (!is_open()) {
      cmd.consume();
      return;
    }
    cmd.retry_with("terminal is open, expected closed");
  }
};

struct HandleExpectTerminalInputCommand : System<PendingE2ECommand> {
  terminal::Console &console;
  explicit HandleExpectTerminalInputCommand(terminal::Console &c) : console(c) {}
  void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("expect_terminal_input")) return;
    if (!cmd.has_args(1)) {
      cmd.fail("expect_terminal_input requires: <text>");
      return;
    }
    const std::string want = join_args(cmd);
    if (console.input == want) {
      cmd.consume();
      return;
    }
    cmd.retry_with(std::format("terminal input is '{}', expected '{}'", console.input, want));
  }
};

struct HandleExpectTerminalOutputContainsCommand : System<PendingE2ECommand> {
  terminal::Console &console;
  explicit HandleExpectTerminalOutputContainsCommand(terminal::Console &c) : console(c) {}
  void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("expect_terminal_output_contains")) return;
    if (!cmd.has_args(1)) {
      cmd.fail("expect_terminal_output_contains requires: <text>");
      return;
    }
    const std::string want = join_args(cmd);
    std::string seen;
    for (const auto &line : console.output()) {
      if (line.text.find(want) != std::string::npos) {
        cmd.consume();
        return;
      }
      if (!seen.empty()) seen += '\n';
      seen += line.text;
    }
    cmd.retry_with(std::format("terminal output does not contain '{}'; output is:\n{}", want, seen));
  }
};

inline void register_terminal_commands(SystemManager &sm, terminal::Console &console,
                                       std::function<bool()> is_open) {
  sm.register_update_system(std::make_unique<HandleExpectTerminalOpenCommand>(is_open));
  sm.register_update_system(std::make_unique<HandleExpectTerminalClosedCommand>(is_open));
  sm.register_update_system(std::make_unique<HandleExpectTerminalInputCommand>(console));
  sm.register_update_system(std::make_unique<HandleExpectTerminalOutputContainsCommand>(console));
}

template <typename OverlayT>
void register_terminal_commands(SystemManager &sm, terminal::Console &console,
                                OverlayT &overlay) {
  register_terminal_commands(sm, console,
                             [&overlay] { return overlay.is_open(); });
}

}  // namespace terminal_commands
}  // namespace testing
}  // namespace afterhours
