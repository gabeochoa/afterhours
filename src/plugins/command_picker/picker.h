#pragma once

#include "../terminal/console.h"

namespace afterhours::command_picker {

struct Entry {
  std::string command;
  std::string label;
  std::string category;
  std::string shortcut;
};

namespace detail {

inline std::optional<int> match_score(std::string_view text, std::string_view query) {
  size_t position = 0;
  int score = 0;
  size_t previous = std::string_view::npos;
  for (unsigned char c : query) {
    if (std::isspace(c)) continue;
    const auto start = position;
    while (position < text.size() &&
           std::tolower(static_cast<unsigned char>(text[position])) != std::tolower(c)) ++position;
    if (position == text.size()) return {};
    score += 10;
    if (position == 0 || !std::isalnum(static_cast<unsigned char>(text[position - 1]))) score += 12;
    if (previous != std::string_view::npos && position == previous + 1) score += 8;
    score -= static_cast<int>(std::min(position - start, size_t{20}));
    previous = position++;
  }
  return score;
}

}

class Picker {
 public:
  std::string query;
  terminal::Result result;

  explicit Picker(std::vector<Entry> entries = {}) { set_entries(std::move(entries)); }

  void set_entries(std::vector<Entry> entries) {
    entries_ = std::move(entries);
    for (auto &entry : entries_)
      if (entry.label.empty()) entry.label = entry.command;
    dirty_ = true;
  }

  bool refresh() {
    if (!dirty_ && previous_query_ == query) return false;
    dirty_ = false;
    previous_query_ = query;
    result = {};
    selected_ = 0;
    matches_.clear();
    std::vector<std::pair<int, size_t>> ranked;
    for (size_t i = 0; i < entries_.size(); ++i) {
      const auto &entry = entries_[i];
      const auto label = detail::match_score(entry.label, query);
      const auto command = detail::match_score(entry.command, query);
      const auto category = detail::match_score(entry.category, query);
      if (!label && !command && !category) continue;
      ranked.emplace_back(std::max({label.value_or(-100000), command.value_or(-100000),
                                    category.value_or(-100000)}), i);
    }
    std::stable_sort(ranked.begin(), ranked.end(), [&](const auto &a, const auto &b) {
      if (a.first != b.first) return a.first > b.first;
      const auto &left = entries_[a.second];
      const auto &right = entries_[b.second];
      if (left.category != right.category) return left.category < right.category;
      return left.label < right.label;
    });
    for (const auto &[score, index] : ranked) matches_.push_back(index);
    return true;
  }

  size_t count() const { return matches_.size(); }
  size_t selected() const { return selected_; }
  const Entry &entry(size_t match) const { return entries_.at(matches_.at(match)); }

  void select(size_t match) {
    if (match >= count()) return;
    selected_ = match;
    result = {};
  }

  void move(bool next) {
    if (!count()) return;
    select(next ? (selected_ + 1) % count() : (selected_ + count() - 1) % count());
  }

  bool activate(terminal::Console &console) {
    refresh();
    if (!count()) return false;
    const auto command = entry(selected_).command;
    const auto parsed = terminal::detail::parse(command);
    if (!parsed.error.empty() || parsed.words.empty()) {
      result = {parsed.error.empty() ? "Empty command" : parsed.error, false};
      return false;
    }
    if (const auto reason = console.command_unavailable_reason(parsed.words.front())) {
      result = {"Unavailable: " + *reason, false};
      return false;
    }
    if (console.execution == terminal::Execution::Queued) {
      const bool queued = console.enqueue(command);
      result = {queued ? "Queued: " + command : "Unable to queue command", queued};
      return queued;
    }
    result = console.execute(command);
    return result.success;
  }

 private:
  std::vector<Entry> entries_;
  std::vector<size_t> matches_;
  size_t selected_ = 0;
  std::string previous_query_;
  bool dirty_ = true;
};

}
