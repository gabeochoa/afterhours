#pragma once

#include "console.h"

namespace afterhours::terminal::detail {

struct Autocomplete {
  std::vector<std::string> matches;
  size_t selected = 0;
  bool explicitly_selected = false;

  void refresh(const Console &console, bool force = false) {
    if (!force && query_ == console.input && revision_ == console.command_revision()) return;
    query_ = console.input;
    revision_ = console.command_revision();
    selected = 0;
    explicitly_selected = false;
    matches = query_.empty() && !force ? std::vector<std::string>{} : console.complete(query_);
    if (force) return;
    const auto parsed = parse(query_);
    if (!parsed.error.empty()) return;
    if (std::any_of(matches.begin(), matches.end(),
                    [&](const auto &match) { return parse(match).words == parsed.words; }))
      matches.clear();
  }

  void move(bool next) {
    if (matches.empty()) return;
    explicitly_selected = true;
    selected = next ? (selected + 1) % matches.size() :
                     (selected + matches.size() - 1) % matches.size();
  }

  void dismiss(const Console &console) {
    query_ = console.input;
    revision_ = console.command_revision();
    matches.clear();
    selected = 0;
    explicitly_selected = false;
  }

  void accept(Console &console) {
    if (matches.empty()) return;
    console.input = matches[selected] + ' ';
    refresh(console);
  }

 private:
  std::string query_;
  size_t revision_ = 0;
};

}
