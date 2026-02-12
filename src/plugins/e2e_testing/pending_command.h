// E2E Testing Framework - Pending Command Component
// ECS component for command dispatch
#pragma once

#include "../../core/key_codes.h"

#include "../../ecs.h"

#include <optional>
#include <string>
#include <vector>

namespace afterhours {
namespace testing {

namespace detail {

// Parse string to type T using appropriate method
template <typename T> T parse_string(const std::string &s) {
  if constexpr (std::is_same_v<T, std::string>)
    return s;
  else if constexpr (std::is_same_v<T, int>)
    return std::stoi(s);
  else if constexpr (std::is_same_v<T, long>)
    return std::stol(s);
  else if constexpr (std::is_same_v<T, float>)
    return std::stof(s);
  else if constexpr (std::is_same_v<T, double>)
    return std::stod(s);
  else if constexpr (std::is_same_v<T, bool>)
    return s == "true" || s == "1" || s == "yes" || s == "on";
  else
    static_assert(sizeof(T) == 0, "No built-in parser for T");
}

} // namespace detail

// Command processing state
enum class CommandState {
  Ready,    // Ready to be processed by handlers
  Retry,    // Handler recognized it but needs to retry next frame
  Consumed, // Fully processed (success or failure)
};

// Component for ALL command dispatch (built-in and custom)
// Systems check is_consumed() and call consume() or fail() when handling
//
// Keep this minimal - just command name and string args.
// Handlers parse what they need from args using the helper methods.
// This supports any custom command without changing the component.
struct PendingE2ECommand : BaseComponent {
  std::string name; // Command name (e.g., "type", "click", "my_custom_cmd")
  std::vector<std::string> args;
  int line_number = 0;
  std::string error_message;
  int frames_alive = 0;                  // How many frames this command has existed
  static constexpr int MAX_FRAMES = 10;  // Commands timeout after 10 frames
  CommandState state = CommandState::Ready;

  // Increment frame counter, returns true if timed out
  bool tick_frame() {
    frames_alive++;
    return frames_alive > MAX_FRAMES;
  }

  // Check if this is a wait command (exempt from timeout)
  bool is_wait_command() const {
    return name == "wait" || name == "wait_frames";
  }

  // Check if this is a specific command
  bool is(const std::string &cmd_name) const { return name == cmd_name; }

  // Mark command as successfully processed
  void consume() { state = CommandState::Consumed; }

  // Mark command to retry next frame (recognized but waiting for condition)
  void retry() { state = CommandState::Retry; }

  // Check if already handled by a previous system
  bool is_consumed() const { return state == CommandState::Consumed; }

  // Check if command is waiting to retry
  bool is_retry() const { return state == CommandState::Retry; }

  // Reset retry state for next frame
  void reset_retry() {
    if (state == CommandState::Retry)
      state = CommandState::Ready;
  }

  // Mark command as failed with error message
  void fail(const std::string &msg) {
    state = CommandState::Consumed;
    success = false;
    error_message = msg;
  }

  // Check if command succeeded (only valid after consumed)
  bool succeeded() const { return success; }

  // Check if command has at least n args
  bool has_args(size_t n) const { return args.size() >= n; }

  // Get raw arg string, or empty if missing
  const std::string &arg(size_t idx) const {
    static const std::string empty;
    return idx < args.size() ? args[idx] : empty;
  }

  // TODO should this be allowed to throw an exception? or auto fail() ?
  // Get arg as type T, or default if missing/invalid
  // Usage: cmd.arg_as<int>(0), cmd.arg_as<float>(1, 0.5f)
  template <typename T> T arg_as(size_t idx, T def = T{}) const {
    if (idx >= args.size())
      return def;
    try {
      return detail::parse_string<T>(args[idx]);
    } catch (...) {
      return def;
    }
  }

  // Get arg as type T with custom parser
  template <typename T, typename Parser>
  T arg_as(size_t idx, T def, Parser parser) const {
    if (idx >= args.size())
      return def;
    try {
      return parser(args[idx]);
    } catch (...) {
      return def;
    }
  }

  // Get arg as optional<T>, nullopt if missing or parse fails
  template <typename T> std::optional<T> maybe_arg_as(size_t idx) const {
    if (idx >= args.size())
      return std::nullopt;
    try {
      return detail::parse_string<T>(args[idx]);
    } catch (...) {
      return std::nullopt;
    }
  }

  // Get arg as optional<T> with custom parser
  template <typename T, typename Parser>
  std::optional<T> maybe_arg_as(size_t idx, Parser parser) const {
    if (idx >= args.size())
      return std::nullopt;
    try {
      return parser(args[idx]);
    } catch (...) {
      return std::nullopt;
    }
  }

  // Get a coordinate arg as pixels. If the raw string ends with '%', it is
  // interpreted as a screen percentage and converted using screen_dim (the
  // screen width or height depending on the axis).
  // Usage: cmd.coord_arg(0, screen_w)  // x-axis
  //        cmd.coord_arg(1, screen_h)  // y-axis
  float coord_arg(size_t idx, float screen_dim) const {
    if (idx >= args.size())
      return 0.f;
    const std::string &s = args[idx];
    if (!s.empty() && s.back() == '%') {
      float pct = std::stof(s.substr(0, s.size() - 1));
      return (pct / 100.0f) * screen_dim;
    }
    return std::stof(s);
  }

private:
  bool consumed = false;
  bool success = true;
};

} // namespace testing
} // namespace afterhours
