#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace afterhours {

/// Base interface for reversible commands.
/// Template parameter State is the type being modified.
template <typename State> struct Command {
  virtual ~Command() = default;

  /// Apply this command to the state
  virtual void execute(State &state) = 0;

  /// Reverse this command
  virtual void undo(State &state) = 0;

  /// Human-readable description for UI (e.g., "Undo: Insert text")
  virtual std::string description() const { return "Command"; }

  /// Can this command be merged with another?
  /// Used for combining sequential typing into one undo step.
  virtual bool can_merge_with(const Command &) const { return false; }

  /// Merge another command into this one.
  /// Called only if can_merge_with returned true.
  virtual void merge_with(Command &) {}
};

/// Generic undo/redo stack.
/// Works with any state type - text buffers, level editors, settings, etc.
template <typename State> struct CommandHistory {
  using CommandPtr = std::unique_ptr<Command<State>>;

  std::vector<CommandPtr> undo_stack;
  std::vector<CommandPtr> redo_stack;
  size_t max_depth = 100;

  CommandHistory() = default;
  explicit CommandHistory(size_t depth) : max_depth(depth) {}

  /// Execute a command and record it for undo
  void execute(CommandPtr cmd, State &state) {
    cmd->execute(state);
    push(std::move(cmd));
  }

  /// Record a command without executing it.
  /// Use when the action was already performed externally.
  void push(CommandPtr cmd) {
    // Try merging with previous command
    if (!undo_stack.empty() && undo_stack.back()->can_merge_with(*cmd)) {
      undo_stack.back()->merge_with(*cmd);
    } else {
      undo_stack.push_back(std::move(cmd));
      if (undo_stack.size() > max_depth) {
        undo_stack.erase(undo_stack.begin());
      }
    }

    // Clear redo stack on new action
    redo_stack.clear();
  }

  /// Undo the last command
  bool undo(State &state) {
    if (undo_stack.empty())
      return false;

    auto cmd = std::move(undo_stack.back());
    undo_stack.pop_back();
    cmd->undo(state);
    redo_stack.push_back(std::move(cmd));
    return true;
  }

  /// Redo the last undone command
  bool redo(State &state) {
    if (redo_stack.empty())
      return false;

    auto cmd = std::move(redo_stack.back());
    redo_stack.pop_back();
    cmd->execute(state);
    undo_stack.push_back(std::move(cmd));
    return true;
  }

  [[nodiscard]] bool can_undo() const { return !undo_stack.empty(); }
  [[nodiscard]] bool can_redo() const { return !redo_stack.empty(); }
  [[nodiscard]] size_t undo_count() const { return undo_stack.size(); }
  [[nodiscard]] size_t redo_count() const { return redo_stack.size(); }

  /// Get description of next undo action (for UI: "Undo: Insert text")
  [[nodiscard]] std::string next_undo_description() const {
    if (undo_stack.empty())
      return "";
    return undo_stack.back()->description();
  }

  /// Get description of next redo action
  [[nodiscard]] std::string next_redo_description() const {
    if (redo_stack.empty())
      return "";
    return redo_stack.back()->description();
  }

  void clear() {
    undo_stack.clear();
    redo_stack.clear();
  }
};

/// Simple command using lambdas (for when you don't need a full class).
/// Useful for prototyping, simple state changes, or one-off commands.
///
/// Example:
/// @code
///   CommandHistory<GameState> history;
///   int old_health = player.health;
///   history.execute(
///     make_command<GameState>(
///       [&](GameState& s) { player.health = 100; },
///       [&, old_health](GameState& s) { player.health = old_health; },
///       "Set health to 100"
///     ),
///     game_state
///   );
/// @endcode
template <typename State> class LambdaCommand : public Command<State> {
public:
  LambdaCommand(std::function<void(State &)> do_fn,
                std::function<void(State &)> undo_fn, std::string desc)
      : do_fn_(std::move(do_fn)), undo_fn_(std::move(undo_fn)),
        description_(std::move(desc)) {}

  void execute(State &state) override { do_fn_(state); }
  void undo(State &state) override { undo_fn_(state); }
  std::string description() const override { return description_; }

private:
  std::function<void(State &)> do_fn_;
  std::function<void(State &)> undo_fn_;
  std::string description_;
};

/// Helper factory function to create lambda commands.
/// Simplifies creation of simple commands without defining a class.
///
/// @param do_fn Lambda that performs the action (receives State&)
/// @param undo_fn Lambda that reverses the action (receives State&)
/// @param desc Human-readable description for UI
/// @return Unique pointer to Command<State>
///
/// Example:
/// @code
///   auto cmd = make_command<EditorState>(
///     [](auto& s) { s.cursor_x++; },
///     [](auto& s) { s.cursor_x--; },
///     "Move cursor right"
///   );
///   history.execute(std::move(cmd), editor_state);
/// @endcode
template <typename State>
inline std::unique_ptr<Command<State>>
make_command(std::function<void(State &)> do_fn,
             std::function<void(State &)> undo_fn, std::string desc) {
  return std::make_unique<LambdaCommand<State>>(
      std::move(do_fn), std::move(undo_fn), std::move(desc));
}

} // namespace afterhours
