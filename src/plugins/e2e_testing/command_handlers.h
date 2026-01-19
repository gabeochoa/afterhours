// E2E Testing Framework - Built-in Command Handlers
// Systems that process PendingE2ECommand entities
#pragma once

#include "pending_command.h"
#include "test_input.h"
#include "visible_text.h"

#include "../../logging.h"

#include <atomic>
#include <format>
#include <functional>

namespace afterhours {
namespace testing {

// Handle 'type "text"' command - types characters
struct HandleTypeCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("type"))
      return;
    if (!cmd.has_args(1)) {
      cmd.fail("type requires text argument");
      return;
    }

    // Clear any leftover chars before pushing new ones
    test_input::clear_queue();
    for (char c : cmd.arg(0)) {
      test_input::push_char(c);
    }
    cmd.consume();
  }
};

// Handle 'key COMBO' command - presses key combo
struct HandleKeyCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("key"))
      return;
    if (cmd.args.empty()) {
      cmd.fail("key command requires argument");
      return;
    }

    auto combo = parse_key_combo(cmd.args[0]);
    if (combo.ctrl)
      input_injector::set_key_down(keys::LEFT_CONTROL);
    if (combo.shift)
      input_injector::set_key_down(keys::LEFT_SHIFT);
    if (combo.alt)
      input_injector::set_key_down(keys::LEFT_ALT);
    input_injector::set_key_down(combo.key);
    test_input::push_key(combo.key);

    cmd.consume();
  }
};

// Handle 'click x y' command - clicks at coordinates
struct HandleClickCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("click"))
      return;
    if (!cmd.has_args(2)) {
      cmd.fail("click requires x y arguments");
      return;
    }

    test_input::simulate_click(cmd.arg_as<float>(0), cmd.arg_as<float>(1));
    cmd.consume();
  }
};

// Handle 'double_click x y' command
struct HandleDoubleClickCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("double_click"))
      return;
    if (!cmd.has_args(2)) {
      cmd.fail("double_click requires x y arguments");
      return;
    }

    test_input::simulate_click(cmd.arg_as<float>(0), cmd.arg_as<float>(1));
    // Note: Full double-click needs frame delay; simplified here
    cmd.consume();
  }
};

// Handle 'drag x1 y1 x2 y2' command
struct HandleDragCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("drag"))
      return;
    if (!cmd.has_args(4)) {
      cmd.fail("drag requires x1 y1 x2 y2 arguments");
      return;
    }

    test_input::simulate_click(cmd.arg_as<float>(0), cmd.arg_as<float>(1));
    input_injector::set_mouse_position(cmd.arg_as<float>(2),
                                       cmd.arg_as<float>(3));
    cmd.consume();
  }
};

// Handle 'mouse_move x y' command
struct HandleMouseMoveCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("mouse_move"))
      return;
    if (!cmd.has_args(2)) {
      cmd.fail("mouse_move requires x y arguments");
      return;
    }

    test_input::set_mouse_position(cmd.arg_as<float>(0), cmd.arg_as<float>(1));
    cmd.consume();
  }
};

// Handle 'wait N' command - waits N seconds (timing handled by E2ERunner)
struct HandleWaitCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("wait"))
      return;
    cmd.consume(); // Wait timing is handled by E2ERunner
  }
};

// Handle 'wait_frames N' command - waits N frames (timing handled by E2ERunner)
struct HandleWaitFramesCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("wait_frames"))
      return;
    cmd.consume(); // Wait timing is handled by E2ERunner
  }
};

// Handle 'expect_text "text"' command - checks visible text
// This command retries across frames until text is found or timeout occurs
struct HandleExpectTextCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("expect_text"))
      return;
    if (cmd.args.empty()) {
      cmd.fail("expect_text requires argument");
      return;
    }

    auto &registry = VisibleTextRegistry::instance();
    if (registry.contains(cmd.args[0])) {
      cmd.consume();
    } else {
      // Mark for retry - text might appear after rendering
      cmd.retry();
    }
  }
};

// Handle 'screenshot name' command - takes screenshot
struct HandleScreenshotCommand : System<PendingE2ECommand> {
  using ScreenshotFn = std::function<void(const std::string &)>;

  explicit HandleScreenshotCommand(ScreenshotFn fn)
      : screenshot_fn_(std::move(fn)) {}

  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("screenshot"))
      return;

    if (screenshot_fn_ && !cmd.args.empty()) {
      screenshot_fn_(cmd.args[0]);
    }
    cmd.consume();
  }

private:
  ScreenshotFn screenshot_fn_;
};

// Handle 'reset_test_state' command - resets test state between tests
// Clears the visible text registry and resets all injected input
struct HandleResetTestStateCommand : System<PendingE2ECommand> {
  using ResetFn = std::function<void()>;

  explicit HandleResetTestStateCommand(ResetFn fn = nullptr)
      : on_reset_(std::move(fn)) {}

  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("reset_test_state"))
      return;

    if (on_reset_) {
      on_reset_();
    }
    test_input::reset_all();
    VisibleTextRegistry::instance().clear();
    cmd.consume();
  }

private:
  ResetFn on_reset_;
};

// Warn about unhandled commands (runs after all other handlers)
struct HandleUnknownCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || cmd.is_retry())
      return; // Skip consumed or retry-pending commands

    log_warn("Unhandled E2E command: '{}' (line {})", cmd.name,
             cmd.line_number);
    cmd.consume();
  }
};

// Global error counter for tracking command failures
namespace detail {
inline std::atomic<int> &command_error_count() {
  static std::atomic<int> count{0};
  return count;
}
} // namespace detail

inline int get_command_error_count() { return detail::command_error_count(); }
inline void reset_command_error_count() { detail::command_error_count() = 0; }

// Cleanup system - removes processed command entities (runs last)
struct E2ECommandCleanupSystem : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &e, PendingE2ECommand &cmd,
                             float) override {
    if (cmd.is_consumed()) {
      // Track failures before cleanup
      if (!cmd.succeeded()) {
        detail::command_error_count()++;
        log_warn("[E2E ERROR] {} (line {}): {}", cmd.name, cmd.line_number,
                 cmd.error_message);
      }
      e.cleanup = true;
      return;
    }

    // Check for command timeout (wait commands are exempt)
    if (!cmd.is_wait_command() && cmd.tick_frame()) {
      std::string error_msg;
      if (cmd.name == "expect_text" && !cmd.args.empty()) {
        // Provide more helpful error for expect_text timeout
        auto &registry = VisibleTextRegistry::instance();
        error_msg = std::format(
            "Text not found: '{}'. Visible: {:.200}", cmd.args[0],
            registry.get_all().empty() ? "(empty)" : registry.get_all());
      } else {
        error_msg = std::format("Command '{}' timed out after {} frames",
                                cmd.name, PendingE2ECommand::MAX_FRAMES);
      }
      cmd.fail(error_msg);
      log_warn("[TIMEOUT] {} (line {}): {}", cmd.name, cmd.line_number,
               error_msg);
      e.cleanup = true;
      return;
    }

    // Reset retry state so command can be re-evaluated next frame
    cmd.reset_retry();
  }
};

} // namespace testing
} // namespace afterhours
