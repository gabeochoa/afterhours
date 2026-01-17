// Command History Plugin for Afterhours
// Provides generic undo/redo functionality using the Command pattern.
// Useful for: level editors, paint programs, form inputs, any reversible actions.
//
// To integrate into Afterhours:
// 1. Add this as a plugin in src/plugins/command_history.h
// 2. Users define their own Command subclasses for their domain

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace afterhours {

/// Base class for undoable commands
/// Users should subclass this for their specific actions
class Command {
public:
  virtual ~Command() = default;
  
  /// Execute the command (do the action)
  virtual void execute() = 0;
  
  /// Undo the command (reverse the action)
  virtual void undo() = 0;
  
  /// Human-readable description for UI/debugging
  virtual std::string description() const { return "Command"; }
  
  /// Whether this command can be merged with the previous one
  /// (e.g., consecutive typing can become one "Insert text" command)
  virtual bool can_merge_with(const Command& /*other*/) const { return false; }
  
  /// Merge another command into this one (called if can_merge_with returns true)
  virtual void merge_with(Command& /*other*/) {}
};

/// Manages undo/redo stacks
class CommandHistory {
public:
  CommandHistory(std::size_t max_history = 100) : max_history_(max_history) {}
  
  /// Execute a command and add it to history
  void execute(std::unique_ptr<Command> cmd) {
    cmd->execute();
    push(std::move(cmd));
  }
  
  /// Add a command to history without executing it
  /// (use when the action was already performed)
  void push(std::unique_ptr<Command> cmd) {
    // Try to merge with previous command
    if (!undo_stack_.empty() && 
        undo_stack_.back()->can_merge_with(*cmd)) {
      undo_stack_.back()->merge_with(*cmd);
    } else {
      undo_stack_.push_back(std::move(cmd));
      
      // Enforce max history size
      if (undo_stack_.size() > max_history_) {
        undo_stack_.erase(undo_stack_.begin());
      }
    }
    
    // Clear redo stack on new action
    redo_stack_.clear();
  }
  
  /// Undo the last command
  void undo() {
    if (undo_stack_.empty()) return;
    
    auto cmd = std::move(undo_stack_.back());
    undo_stack_.pop_back();
    
    cmd->undo();
    redo_stack_.push_back(std::move(cmd));
  }
  
  /// Redo the last undone command
  void redo() {
    if (redo_stack_.empty()) return;
    
    auto cmd = std::move(redo_stack_.back());
    redo_stack_.pop_back();
    
    cmd->execute();
    undo_stack_.push_back(std::move(cmd));
  }
  
  bool can_undo() const { return !undo_stack_.empty(); }
  bool can_redo() const { return !redo_stack_.empty(); }
  
  std::size_t undo_count() const { return undo_stack_.size(); }
  std::size_t redo_count() const { return redo_stack_.size(); }
  
  /// Get description of next undo action (for UI: "Undo: Insert text")
  std::string next_undo_description() const {
    if (undo_stack_.empty()) return "";
    return undo_stack_.back()->description();
  }
  
  /// Get description of next redo action
  std::string next_redo_description() const {
    if (redo_stack_.empty()) return "";
    return redo_stack_.back()->description();
  }
  
  /// Clear all history
  void clear() {
    undo_stack_.clear();
    redo_stack_.clear();
  }
  
  /// Set maximum history size
  void set_max_history(std::size_t max) {
    max_history_ = max;
    while (undo_stack_.size() > max_history_) {
      undo_stack_.erase(undo_stack_.begin());
    }
  }

private:
  std::vector<std::unique_ptr<Command>> undo_stack_;
  std::vector<std::unique_ptr<Command>> redo_stack_;
  std::size_t max_history_;
};

//-----------------------------------------------------------------------------
// Convenience: Lambda-based command for simple cases
//-----------------------------------------------------------------------------

/// Simple command using lambdas (for when you don't need a full class)
class LambdaCommand : public Command {
public:
  LambdaCommand(std::function<void()> do_fn, 
                std::function<void()> undo_fn,
                std::string desc = "Action")
    : do_fn_(std::move(do_fn))
    , undo_fn_(std::move(undo_fn))
    , description_(std::move(desc)) {}
  
  void execute() override { do_fn_(); }
  void undo() override { undo_fn_(); }
  std::string description() const override { return description_; }

private:
  std::function<void()> do_fn_;
  std::function<void()> undo_fn_;
  std::string description_;
};

/// Helper to create lambda commands
inline std::unique_ptr<Command> make_command(
    std::function<void()> do_fn,
    std::function<void()> undo_fn,
    std::string desc = "Action") {
  return std::make_unique<LambdaCommand>(
      std::move(do_fn), std::move(undo_fn), std::move(desc));
}

//-----------------------------------------------------------------------------
// Example usage:
//-----------------------------------------------------------------------------
/*
CommandHistory history;

// Using lambda command for simple cases
int value = 10;
int old_value = value;
history.execute(make_command(
    [&]() { value = 20; },
    [&, old_value]() { value = old_value; },
    "Set value to 20"
));

// Undo
history.undo();  // value is now 10 again

// Redo
history.redo();  // value is now 20 again

// For complex commands, subclass Command:
class MoveEntityCommand : public Command {
public:
  MoveEntityCommand(Entity& entity, Vector2 from, Vector2 to)
    : entity_(entity), from_(from), to_(to) {}
  
  void execute() override { entity_.position = to_; }
  void undo() override { entity_.position = from_; }
  std::string description() const override { return "Move entity"; }
  
private:
  Entity& entity_;
  Vector2 from_, to_;
};
*/

} // namespace afterhours

