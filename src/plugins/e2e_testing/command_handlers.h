// E2E Testing Framework - Built-in Command Handlers
// Systems that process PendingE2ECommand entities
#pragma once

#include "pending_command.h"
#include "test_input.h"
#include "visible_text.h"

#include "../../logging.h"
#include "../window_manager.h"

#include <atomic>
#include <format>
#include <functional>

namespace afterhours {
namespace testing {

// Fetch current screen dimensions for resolving %-based coordinates.
inline std::pair<float, float> e2e_screen_size() {
  auto *pcr = EntityHelper::get_singleton_cmp<
      window_manager::ProvidesCurrentResolution>();
  if (pcr)
    return {static_cast<float>(pcr->width()),
            static_cast<float>(pcr->height())};
  return {1280.f, 720.f}; // fallback
}

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
// Helper to track pending key releases across frames
namespace key_release_detail {
  inline bool pending_ctrl = false;
  inline bool pending_shift = false;
  inline bool pending_alt = false;
  inline int pending_key = 0;
  inline int release_countdown = 0;
  
  inline void reset() {
    pending_ctrl = false;
    pending_shift = false;
    pending_alt = false;
    pending_key = 0;
    release_countdown = 0;
  }
}

// System to release keys after the app has processed them
// This runs every frame and counts down to release
struct HandleKeyReleaseSystem : System<> {
  virtual void once(float) override {
    using namespace key_release_detail;
    if (release_countdown > 0) {
      release_countdown--;
      if (release_countdown == 0) {
        // Release all pending keys
        if (pending_ctrl) {
          input_injector::set_key_up(keys::LEFT_CONTROL);
          pending_ctrl = false;
        }
        if (pending_shift) {
          input_injector::set_key_up(keys::LEFT_SHIFT);
          pending_shift = false;
        }
        if (pending_alt) {
          input_injector::set_key_up(keys::LEFT_ALT);
          pending_alt = false;
        }
        if (pending_key > 0) {
          input_injector::set_key_up(pending_key);
          pending_key = 0;
        }
      }
    }
  }
};

struct HandleKeyCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("key"))
      return;
    if (cmd.args.empty()) {
      cmd.fail("key command requires argument");
      return;
    }

    using namespace key_release_detail;
    
    auto combo = parse_key_combo(cmd.args[0]);
    
    // Set modifiers down for this frame
    if (combo.ctrl)
      input_injector::set_key_down(keys::LEFT_CONTROL);
    if (combo.shift)
      input_injector::set_key_down(keys::LEFT_SHIFT);
    if (combo.alt)
      input_injector::set_key_down(keys::LEFT_ALT);
    
    // Push the key to queue (for polling APIs)
    test_input::push_key(combo.key);
    
    // Mark key as pressed (for synthetic press detection)
    input_injector::set_key_down(combo.key);

    // Schedule release in 2 frames (gives app time to process)
    pending_ctrl = combo.ctrl;
    pending_shift = combo.shift;
    pending_alt = combo.alt;
    pending_key = combo.key;
    release_countdown = 2;

    cmd.consume();
  }
};

// Handle 'click x y' command - clicks at coordinates
// Coordinates support '%' suffix for screen-relative positioning.
struct HandleClickCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("click"))
      return;
    if (!cmd.has_args(2)) {
      cmd.fail("click requires x y arguments");
      return;
    }

    auto [sw, sh] = e2e_screen_size();
    test_input::simulate_click(cmd.coord_arg(0, sw), cmd.coord_arg(1, sh));
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

    auto [sw, sh] = e2e_screen_size();
    test_input::simulate_click(cmd.coord_arg(0, sw), cmd.coord_arg(1, sh));
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

    auto [sw, sh] = e2e_screen_size();
    test_input::simulate_click(cmd.coord_arg(0, sw), cmd.coord_arg(1, sh));
    input_injector::set_mouse_position(cmd.coord_arg(2, sw),
                                       cmd.coord_arg(3, sh));
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

    auto [sw, sh] = e2e_screen_size();
    test_input::set_mouse_position(cmd.coord_arg(0, sw), cmd.coord_arg(1, sh));
    cmd.consume();
  }
};

// Handle 'mouse_down x y' command - press mouse at position (no release).
struct HandleMouseDownCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("mouse_down"))
      return;
    if (!cmd.has_args(2)) {
      cmd.fail("mouse_down requires x y arguments");
      return;
    }

    auto [sw, sh] = e2e_screen_size();
    float x = cmd.coord_arg(0, sw);
    float y = cmd.coord_arg(1, sh);
    test_input::set_mouse_position(x, y);
    auto &m = input_injector::detail::mouse;
    m.left_down = true;
    m.just_pressed = true;
    m.press_frames = 0;
    m.active = true;
    cmd.consume();
  }
};

// Handle 'mouse_up' command - release mouse button.
struct HandleMouseUpCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("mouse_up"))
      return;
    test_input::simulate_mouse_release();
    cmd.consume();
  }
};

// Handle 'scroll_wheel dx dy' command - inject a synthetic mouse wheel event.
// The wheel delta is consumed by the next call to get_mouse_wheel_move_v().
// dx/dy are float values (positive = right/up in natural scrolling).
struct HandleScrollWheelCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("scroll_wheel"))
      return;
    if (!cmd.has_args(2)) {
      cmd.fail("scroll_wheel requires dx dy arguments");
      return;
    }
    float dx = cmd.arg_as<float>(0);
    float dy = cmd.arg_as<float>(1);
    input_injector::set_mouse_wheel(dx, dy);
    cmd.consume();
  }
};

// Handle 'drag_to x1 y1 x2 y2' command - multi-frame press→move→release.
// Spreads the operation across 3 frames so UI systems see proper state
// transitions (just_pressed on frame 1, held+moved on frame 2, released on
// frame 3).
struct HandleDragToCommand : System<PendingE2ECommand> {
  int phase = 0; // 0=press, 1=move, 2=release

  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("drag_to"))
      return;
    if (!cmd.has_args(4)) {
      cmd.fail("drag_to requires x1 y1 x2 y2 arguments");
      return;
    }

    // Reset phase for a fresh command (guards against stale state from a
    // prior drag_to that timed out mid-sequence).
    if (!cmd.is_retry())
      phase = 0;

    auto [sw, sh] = e2e_screen_size();

    switch (phase) {
    case 0: {
      // Frame 0: press at start position
      float x1 = cmd.coord_arg(0, sw);
      float y1 = cmd.coord_arg(1, sh);
      test_input::set_mouse_position(x1, y1);
      auto &m = input_injector::detail::mouse;
      m.left_down = true;
      m.just_pressed = true;
      m.press_frames = 0; // no auto-release
      m.active = true;
      phase = 1;
      cmd.retry(); // Mark as in-progress so unknown handler skips it
      break;
    }
    case 1: {
      // Frame 1: move to end position (mouse still held)
      float x2 = cmd.coord_arg(2, sw);
      float y2 = cmd.coord_arg(3, sh);
      test_input::set_mouse_position(x2, y2);
      phase = 2;
      cmd.retry(); // Mark as in-progress so unknown handler skips it
      break;
    }
    case 2:
      // Frame 2: release
      test_input::simulate_mouse_release();
      phase = 0; // reset for next drag_to command
      cmd.consume();
      break;
    }
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

// Handle 'expect_no_text "text"' command - asserts text is NOT visible
// Succeeds immediately if text is absent. Retries a few frames to allow
// rendering to settle, then succeeds if still absent.
struct HandleExpectNoTextCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("expect_no_text"))
      return;
    if (cmd.args.empty()) {
      cmd.fail("expect_no_text requires argument");
      return;
    }

    auto &registry = VisibleTextRegistry::instance();
    if (registry.contains(cmd.args[0])) {
      cmd.fail(std::format(
          "expect_no_text failed: '{}' IS visible but should not be",
          cmd.args[0]));
    } else {
      cmd.consume();
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
    key_release_detail::reset();  
    VisibleTextRegistry::instance().clear();
    cmd.consume();
  }

private:
  ResetFn on_reset_;
};

// Handle 'resize w h' command - resizes the window/viewport
// Updates ProvidesCurrentResolution singleton and calls set_window_size().
// Screen systems will pick up the new dimensions on the next frame.
struct HandleResizeCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("resize"))
      return;
    if (!cmd.has_args(2)) {
      cmd.fail("resize requires width height arguments");
      return;
    }

    int w = cmd.arg_as<int>(0);
    int h = cmd.arg_as<int>(1);

    if (w <= 0 || h <= 0) {
      cmd.fail(std::format("resize: invalid dimensions {}x{}", w, h));
      return;
    }

    // Update the ECS resolution singleton (authoritative source for UI layout)
    auto *pcr = EntityHelper::get_singleton_cmp<
        window_manager::ProvidesCurrentResolution>();
    if (pcr) {
      pcr->current_resolution.width = w;
      pcr->current_resolution.height = h;
      pcr->should_refetch = false; // Prevent CollectCurrentResolution from overwriting
    }

    // Physically resize the window (no-op in headless mode)
    window_manager::set_window_size(w, h);

    cmd.consume();
  }
};

// Fail on unhandled commands (runs after all other handlers)
struct HandleUnknownCommand : System<PendingE2ECommand> {
  virtual void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || cmd.is_retry())
      return; // Skip consumed or retry-pending commands

    cmd.fail(std::format("Unknown command: '{}'", cmd.name));
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
      detail::command_error_count()++;
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
