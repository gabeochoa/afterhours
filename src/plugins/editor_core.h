#pragma once

#include <cstddef>
#include <optional>
#include <set>
#include <string>
#include <utility>

#include "../core/entity.h"
#include "command_history.h"

namespace afterhours::editor {

template <typename State, typename Id = EntityID> struct EditorCore {
  CommandHistory<State> history;

  explicit EditorCore(std::size_t max_depth = 100) : history(max_depth) {}

  void select(Id id, bool additive = false) {
    if (!additive) selection.clear();
    selection.insert(id);
    primary_id = id;
  }

  void toggle(Id id) {
    if (selection.erase(id)) {
      if (primary_id == id)
        primary_id = selection.empty() ? std::nullopt
                                      : std::optional<Id>(*selection.begin());
    } else {
      selection.insert(id);
      primary_id = id;
    }
  }

  void deselect(Id id) { toggle_off(id); }

  void clear_selection() { selection.clear(); primary_id.reset(); }

  [[nodiscard]] bool is_selected(Id id) const { return selection.contains(id); }

  [[nodiscard]] const std::set<Id> &selected() const { return selection; }

  [[nodiscard]] std::optional<Id> primary() const { return primary_id; }

  [[nodiscard]] std::size_t selection_size() const { return selection.size(); }

  void execute(typename CommandHistory<State>::CommandPtr cmd, State &state) {
    history.execute(std::move(cmd), state);
    ++revision;
  }

  void record(typename CommandHistory<State>::CommandPtr cmd) {
    history.push(std::move(cmd));
    ++revision;
  }

  void mark_dirty() { ++revision; }

  bool undo(State &state) {
    if (!history.undo(state)) return false;
    --revision;
    return true;
  }

  bool redo(State &state) {
    if (!history.redo(state)) return false;
    ++revision;
    return true;
  }

  [[nodiscard]] bool dirty() const { return revision != saved_revision; }

  [[nodiscard]] bool can_undo() const { return history.can_undo(); }

  [[nodiscard]] bool can_redo() const { return history.can_redo(); }

  [[nodiscard]] std::string undo_label() const {
    return history.next_undo_description();
  }

  [[nodiscard]] std::string redo_label() const {
    return history.next_redo_description();
  }

  void mark_saved() { saved_revision = revision; }

private:
  void toggle_off(Id id) {
    if (!selection.erase(id)) return;
    if (primary_id == id)
      primary_id = selection.empty() ? std::nullopt
                                    : std::optional<Id>(*selection.begin());
  }

  std::set<Id> selection;
  std::optional<Id> primary_id;
  long long revision = 0;
  long long saved_revision = 0;
};

} // namespace afterhours::editor
