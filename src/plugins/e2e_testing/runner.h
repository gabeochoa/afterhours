// E2E Testing Framework - Script Runner
// DSL parser and command dispatch
#pragma once

#include "command_handlers.h"

#include "../../logging.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string_view>

namespace afterhours {
namespace testing {

struct ScriptResult {
  std::string name;
  std::string path;
  bool expected_to_pass = true;
  bool passed = true;
  int error_count = 0;
  int validation_failures = 0;
};

struct ParsedCommand {
  std::string name; // Command name (e.g., "type", "click", "my_custom_cmd")
  std::vector<std::string> args;
  int line_number = 0;
  float wait_seconds = 0.0f; // Time to wait after this command
};

namespace detail {

// constexpr helper to check if a value is in an array
template <typename T, std::size_t N>
constexpr bool contains(const std::array<T, N> &arr, std::string_view val) {
  for (const auto &item : arr) {
    if (item == val)
      return true;
  }
  return false;
}

// Command categories for argument parsing
// clang-format off
constexpr std::array<std::string_view, 3> coord_commands = {
    "click", "double_click", "mouse_move"
};

constexpr std::array<std::string_view, 13> single_arg_commands = {
    "key", "select_all", "screenshot", "arrow",
    "action", "hold", "release", "click_ui",
    "click_text", "click_button", "focus_ui",
    "toggle_checkbox", "expect_focused"
};

constexpr std::array<std::string_view, 4> two_arg_commands = {
    "set_slider", "select_dropdown", "expect_slider", "expect_checkbox"
};

constexpr std::array<std::string_view, 6> no_arg_commands = {
    "reset_test_state", "reset", "tab", "shift_tab", "enter", "escape"
};
// clang-format on

} // namespace detail

inline std::vector<ParsedCommand> parse_script(const std::string &path) {
  std::vector<ParsedCommand> cmds;
  std::ifstream file(path);
  if (!file.is_open())
    return cmds;

  std::string line;
  int line_num = 0;

  while (std::getline(file, line)) {
    line_num++;
    size_t start = line.find_first_not_of(" \t");
    if (start == std::string::npos)
      continue;
    line = line.substr(start);
    if (line.empty() || line[0] == '#')
      continue;

    ParsedCommand cmd;
    cmd.line_number = line_num;
    std::istringstream iss(line);
    iss >> cmd.name;

    auto parse_quoted = [&]() {
      std::string result;
      std::getline(iss >> std::ws, result);
      if (!result.empty() && result.front() == '"')
        result = result.substr(1);
      if (!result.empty() && result.back() == '"')
        result.pop_back();
      return result;
    };

    // Determine command type and parse accordingly
    // Wait times in seconds (assuming ~60fps for frame-based defaults)
    constexpr float frame = 1.0f / 60.0f;

    if (cmd.name == "type") {
      cmd.args.push_back(parse_quoted());
      cmd.wait_seconds = (cmd.args[0].size() + 2) * frame;
    } else if (cmd.name == "key" || cmd.name == "select_all") {
      std::string arg;
      iss >> arg;
      if (cmd.name == "select_all") {
        arg = "CTRL+A";
        cmd.name = "key";  // Normalize to key command
      }
      cmd.args.push_back(arg);
      cmd.wait_seconds = 3 * frame;
    } else if (detail::contains(detail::coord_commands, cmd.name)) {
      std::string x_str, y_str;
      iss >> x_str >> y_str;
      cmd.args.push_back(x_str);
      cmd.args.push_back(y_str);
      cmd.wait_seconds = (cmd.name == "double_click") ? 4 * frame
                         : (cmd.name == "mouse_move") ? 1 * frame
                                                      : 2 * frame;
    } else if (cmd.name == "drag") {
      std::string x1, y1, x2, y2;
      iss >> x1 >> y1 >> x2 >> y2;
      cmd.args.push_back(x1);
      cmd.args.push_back(y1);
      cmd.args.push_back(x2);
      cmd.args.push_back(y2);
      cmd.wait_seconds = 5 * frame;
    } else if (cmd.name == "wait") {
      // Wait in seconds (default 1 second)
      float seconds = 1.0f;
      iss >> seconds;
      if (seconds <= 0)
        seconds = 1.0f;
      cmd.args.push_back(std::to_string(seconds));
      cmd.wait_seconds = seconds;
    } else if (cmd.name == "wait_frames") {
      int frames = 1;
      iss >> frames;
      if (frames <= 0)
        frames = 1;
      cmd.wait_seconds = frames * frame;
    } else if (cmd.name == "validate") {
      std::string rest;
      std::getline(iss >> std::ws, rest);
      size_t eq = rest.find('=');
      if (eq != std::string::npos) {
        cmd.args.push_back(rest.substr(0, eq));
        cmd.args.push_back(rest.substr(eq + 1));
      }
      cmd.wait_seconds = 1 * frame;
    } else if (cmd.name == "expect_text") {
      cmd.args.push_back(parse_quoted());
      cmd.wait_seconds = 1 * frame;
    } else if (cmd.name == "screenshot") {
      std::string name;
      iss >> name;
      cmd.args.push_back(name);
      cmd.wait_seconds = 1 * frame;
    } else if (cmd.name == "reset_test_state" || cmd.name == "reset") {
      cmd.name = "reset_test_state"; // Normalize alias
      cmd.wait_seconds = 2 * frame;
    } else if (detail::contains(detail::no_arg_commands, cmd.name)) {
      cmd.wait_seconds = 2 * frame;
    } else if (cmd.name == "arrow") {
      std::string dir;
      iss >> dir;
      cmd.args.push_back(dir);
      cmd.wait_seconds = 2 * frame;
    } else if (detail::contains(detail::single_arg_commands, cmd.name)) {
      std::string arg;
      iss >> arg;
      if (!arg.empty())
        cmd.args.push_back(arg);
      cmd.wait_seconds = 2 * frame;
    } else if (detail::contains(detail::two_arg_commands, cmd.name)) {
      std::string arg1, arg2;
      iss >> arg1 >> arg2;
      if (!arg1.empty())
        cmd.args.push_back(arg1);
      if (!arg2.empty())
        cmd.args.push_back(arg2);
      cmd.wait_seconds = 2 * frame;
    } else {
      // Custom/unknown command - parse all remaining as args
      std::string arg;
      while (iss >> arg) {
        cmd.args.push_back(arg);
      }
      cmd.wait_seconds = 2 * frame;
    }

    cmds.push_back(cmd);
  }
  return cmds;
}

class E2ERunner {
public:
  static constexpr float DEFAULT_TIMEOUT_SECONDS = 10.0f;

  void load_script(const std::string &path) {
    commands_ = parse_script(path);
    script_path_ = path;
    reset();
  }

  void load_scripts_from_directory(const std::string &dir) {
    commands_.clear();
    script_results_.clear();
    reset();

    std::vector<std::string> scripts;
    for (const auto &entry : std::filesystem::directory_iterator(dir)) {
      if (entry.path().extension() == ".e2e") {
        scripts.push_back(entry.path().string());
      }
    }
    std::sort(scripts.begin(), scripts.end());

    for (const auto &script_path : scripts) {
      std::string script_name =
          std::filesystem::path(script_path).stem().string();
      ScriptResult result;
      result.path = script_path;
      result.name = script_name;
      result.expected_to_pass = (script_name.substr(0, 5) != "fail_");
      script_results_.push_back(result);

      auto script_cmds = parse_script(script_path);
      for (auto &cmd : script_cmds) {
        commands_.push_back(cmd);
      }

      // Add reset command between scripts
      ParsedCommand reset_cmd;
      reset_cmd.name = "reset_test_state";
      reset_cmd.wait_seconds = 0.033f; // ~2 frames at 60fps
      commands_.push_back(reset_cmd);
    }

    log_info("[BATCH] Loaded {} scripts with {} commands",
             script_results_.size(), commands_.size());
  }

  void reset() {
    index_ = 0;
    wait_time_ = 0.0f;
    elapsed_time_ = 0.0f;
    finished_ = false;
    failed_ = false;
    timed_out_ = false;
    current_script_idx_ = 0;
    current_script_errors_ = 0;
  }

  void set_timeout(float seconds) { timeout_seconds_ = seconds; }
  void set_timeout_frames(int frames) {
    timeout_seconds_ = frames / 60.0f;
  } // Legacy
  void set_slow_mode(bool enabled, float delay_seconds = 0.5f) {
    slow_mode_ = enabled;
    slow_delay_ = delay_seconds;
  }
  void set_screenshot_callback(std::function<void(const std::string &)> fn) {
    screenshot_fn_ = fn;
  }
  void set_reset_callback(std::function<void()> fn) { clear_fn_ = fn; }
  void set_property_getter(std::function<std::string(const std::string &)> fn) {
    property_getter_ = fn;
  }

  /// Call each frame with delta time (preferred)
  void tick(float dt) {
    if (finished_ || commands_.empty())
      return;
    elapsed_time_ += dt;

    // Timeout check
    if (timeout_seconds_ > 0 && elapsed_time_ > timeout_seconds_) {
      timed_out_ = true;
      failed_ = true;
      finished_ = true;
      current_script_errors_++; // Count timeout as an error
      finalize_current_script();
      log_warn("[TIMEOUT] after {:.2f} seconds", elapsed_time_);
      return;
    }

    // Handle wait between commands
    if (wait_time_ > 0) {
      wait_time_ -= dt;
      return;
    }

    if (index_ >= commands_.size()) {
      finalize_current_script();
      finished_ = true;
      return;
    }

    // Create entity with PendingE2ECommand for this command
    const auto &cmd = commands_[index_];
    dispatch_command(cmd);
    wait_time_ = cmd.wait_seconds;
    if (slow_mode_) {
      wait_time_ += slow_delay_;
    }
    index_++;

    if (index_ >= commands_.size()) {
      finalize_current_script();
      finished_ = true;
    }
  }

  /// Call each frame assuming 60fps (legacy, prefer tick(dt))
  void tick() { tick(1.0f / 60.0f); }

  bool is_finished() const { return finished_; }
  bool has_failed() const {
    if (!script_results_.empty()) {
      for (const auto &sr : script_results_) {
        if (!sr.passed)
          return true;
      }
      return false;
    }
    return failed_;
  }
  bool has_timed_out() const { return timed_out_; }
  bool has_commands() const { return !commands_.empty(); }
  float elapsed_time() const { return elapsed_time_; }

  void print_results() const {
    if (!script_results_.empty()) {
      log_info("============================================");
      log_info("          E2E Batch Test Summary            ");
      log_info("============================================");

      int passed = 0, failed_count = 0;
      for (const auto &sr : script_results_) {
        if (sr.passed) {
          passed++;
        } else {
          failed_count++;
        }
      }

      log_info("Scripts run:    {}", script_results_.size());
      log_info("Scripts passed: {}", passed);
      log_info("Scripts failed: {}", failed_count);
      return;
    }

    if (timed_out_)
      log_warn("[TIMEOUT] after {:.2f} seconds", elapsed_time_);
    log_info("E2E finished in {:.2f} seconds", elapsed_time_);
  }

  // Backward compatibility aliases
  bool isFinished() const { return is_finished(); }
  bool hasFailed() const { return has_failed(); }
  bool hasCommands() const { return has_commands(); }
  void printResults() const { print_results(); }

private:
  void dispatch_command(const ParsedCommand &cmd) {
    auto &entity = EntityHelper::createEntity();
    auto &pending = entity.addComponent<PendingE2ECommand>();
    pending.name = cmd.name;
    pending.args = cmd.args;
    pending.line_number = cmd.line_number;

    // Handle validate specially since it needs property_getter
    if (cmd.name == "validate" && property_getter_ && cmd.args.size() >= 2) {
      std::string actual = property_getter_(cmd.args[0]);
      if (actual == cmd.args[1]) {
        pending.consume();
      } else {
        log_warn("[E2E ERROR] validate (line {}): Expected {}={}, got {}",
                 cmd.line_number, cmd.args[0], cmd.args[1], actual);
        failed_ = true;
        current_script_errors_++;
        pending.consume();  // Mark as handled (failed validation is still "consumed")
      }
    }

    // Handle screenshot if callback set
    if (cmd.name == "screenshot" && screenshot_fn_ && !cmd.args.empty()) {
      screenshot_fn_(cmd.args[0]);
      pending.consume();
    }

    // Handle reset_test_state
    if (cmd.name == "reset_test_state") {
      finalize_current_script();
      if (clear_fn_)
        clear_fn_();
      test_input::reset_all();
      key_release_detail::reset();  // Clear pending key releases
      VisibleTextRegistry::instance().clear();
      current_script_idx_++;
      current_script_errors_ = 0;
      elapsed_time_ = 0.0f;  // Reset timeout for next script
      pending.consume();
    }
  }

  void finalize_current_script() {
    if (script_results_.empty() ||
        current_script_idx_ >= script_results_.size())
      return;

    // Sync with command handler error count (from E2ECommandCleanupSystem)
    int command_errors = get_command_error_count();
    current_script_errors_ += command_errors;
    reset_command_error_count();  // Reset for next script

    auto &result = script_results_[current_script_idx_];
    result.error_count = current_script_errors_;
    bool actually_passed = (current_script_errors_ == 0);

    result.passed = (result.expected_to_pass == actually_passed);

    if (result.passed) {
      log_info("[PASS] {}", result.name);
    } else {
      log_warn("[FAIL] {}", result.name);
    }
  }

  std::vector<ParsedCommand> commands_;
  std::string script_path_;
  std::size_t index_ = 0;
  float wait_time_ = 0.0f;        // Seconds remaining before next command
  float elapsed_time_ = 0.0f;     // Total elapsed time
  float timeout_seconds_ = 10.0f; // Default 10 second timeout
  bool slow_mode_ = false;        // Add delay between commands for visibility
  float slow_delay_ = 0.5f;       // Delay in seconds when slow mode is on
  bool finished_ = false;
  bool failed_ = false;
  bool timed_out_ = false;

  std::vector<ScriptResult> script_results_;
  std::size_t current_script_idx_ = 0;
  int current_script_errors_ = 0;

  std::function<void(const std::string &)> screenshot_fn_;
  std::function<void()> clear_fn_;
  std::function<std::string(const std::string &)> property_getter_;
};

} // namespace testing
} // namespace afterhours
