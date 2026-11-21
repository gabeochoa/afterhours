#pragma once


#include <cstddef>
#include <functional>
#include <optional>
#include <set>
#include <utility>

#include "command_history.h"

namespace afterhours::editor {

using EntityId = long long;

template <typename State, typename Id = EntityId> struct EditorCore {
  using SaveHook = std::function<bool()>;
  using PickCallback = std::function<std::optional<Id>()>;

  CommandHistory<State> history;

  explicit EditorCore(std::size_t max_depth = 100) : history(max_depth) {}

  void select(Id id, bool additive = false) {
    if (!additive) selection_.clear();
    selection_.insert(id);
    primary_ = id;
  }
  void toggle(Id id) {
    if (selection_.erase(id)) {
      if (primary_ == id)
        primary_ = selection_.empty() ? std::nullopt
                                      : std::optional<Id>(*selection_.begin());
    } else {
      selection_.insert(id);
      primary_ = id;
    }
  }
  void deselect(Id id) { toggle_off(id); }
  void clear_selection() { selection_.clear(); primary_.reset(); }
  [[nodiscard]] bool is_selected(Id id) const { return selection_.contains(id); }
  [[nodiscard]] const std::set<Id> &selected() const { return selection_; }
  [[nodiscard]] std::optional<Id> primary() const { return primary_; }
  [[nodiscard]] std::size_t selection_size() const { return selection_.size(); }

  void execute(typename CommandHistory<State>::CommandPtr cmd, State &state) {
    history.execute(std::move(cmd), state);
    ++revision_;
  }
  void record(typename CommandHistory<State>::CommandPtr cmd) {
    history.push(std::move(cmd));
    ++revision_;
  }
  void mark_dirty() { ++revision_; }
  bool undo(State &state) {
    if (!history.undo(state)) return false;
    --revision_;
    return true;
  }
  bool redo(State &state) {
    if (!history.redo(state)) return false;
    ++revision_;
    return true;
  }
  [[nodiscard]] bool dirty() const { return revision_ != saved_revision_; }
  [[nodiscard]] bool can_undo() const { return history.can_undo(); }
  [[nodiscard]] bool can_redo() const { return history.can_redo(); }
  [[nodiscard]] std::string undo_label() const {
    return history.next_undo_description();
  }
  [[nodiscard]] std::string redo_label() const {
    return history.next_redo_description();
  }

  void on_save(SaveHook hook) { save_hook_ = std::move(hook); }
  bool save() {
    if (save_hook_ && !save_hook_()) return false;
    mark_saved();
    return true;
  }
  void mark_saved() { saved_revision_ = revision_; }

  void set_pick_callback(PickCallback cb) { pick_cb_ = std::move(cb); }
  std::optional<Id> pick(bool additive = false) {
    return pick_cb_ ? pick_with(pick_cb_, additive) : std::nullopt;
  }
  std::optional<Id> pick_with(const PickCallback &cb, bool additive = false) {
    std::optional<Id> hit = cb();
    if (hit) select(*hit, additive);
    else if (!additive) clear_selection();
    return hit;
  }

private:
  void toggle_off(Id id) {
    if (!selection_.erase(id)) return;
    if (primary_ == id)
      primary_ = selection_.empty() ? std::nullopt
                                    : std::optional<Id>(*selection_.begin());
  }

  std::set<Id> selection_;
  std::optional<Id> primary_;
  long long revision_ = 0;
  long long saved_revision_ = 0;
  SaveHook save_hook_;
  PickCallback pick_cb_;
};

}
