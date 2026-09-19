#pragma once

#include "commands.h"

#include <algorithm>
#include <deque>
#include <iomanip>
#include <map>
#include <memory>
#include <sstream>

namespace afterhours::terminal {

class Console {
 public:
  std::string input;

  explicit Console(size_t output_limit = 200, size_t history_limit = 100)
      : output_limit_(output_limit), history_limit_(history_limit) {}

  bool add_command(Command command) {
    if (command.name.empty() || !command.run || command.name == "help" ||
        command.name == "clear") return false;
    for (unsigned char c : command.name)
      if (!std::isalnum(c) && c != '_' && c != '-' && c != '.') return false;
    auto name = command.name;
    return commands_.emplace(std::move(name), std::move(command)).second;
  }

  bool add_command(std::unique_ptr<CommandBase> command) {
    if (!command) return false;
    auto owned = std::shared_ptr<CommandBase>(std::move(command));
    return add_command({std::string(owned->name()), std::string(owned->help()),
                        [owned](Arguments args) { return owned->run(args); },
                        owned->completions()});
  }

  bool remove_command(const std::string &name) { return commands_.erase(name) != 0; }
  const std::deque<Result> &output() const { return output_; }
  size_t output_revision() const { return output_revision_; }
  const std::deque<std::string> &history() const { return history_; }
  void clear() { output_.clear(); ++output_revision_; }

  void print(Result result) {
    if (!output_limit_ || result.text.empty()) return;
    ++output_revision_;
    std::istringstream lines(std::move(result.text));
    for (std::string line; std::getline(lines, line);) {
      output_.push_back({std::move(line), result.success});
      if (output_.size() > output_limit_) output_.pop_front();
    }
  }

  Result execute(std::string_view line) {
    auto parsed = detail::parse(line);
    if (parsed.words.empty() && parsed.error.empty()) return {};
    const std::string owned_line(line);
    print({"> " + owned_line});
    if (history_limit_ && (history_.empty() || history_.back() != owned_line)) {
      history_.push_back(owned_line);
      while (history_.size() > history_limit_) history_.pop_front();
    }
    Result result = parsed.error.empty() ? run(Arguments(parsed.words)) : Result{parsed.error, false};
    print(result);
    return result;
  }

  std::vector<std::string> complete(std::string_view line) const {
    const auto split = line.find_first_of(" \t");
    std::vector<std::string> matches;
    if (split == std::string_view::npos) {
      for (const auto &name : {"clear", "help"})
        if (std::string_view(name).starts_with(line)) matches.emplace_back(name);
      for (const auto &[name, command] : commands_)
        if (name.starts_with(line)) matches.push_back(name);
      std::sort(matches.begin(), matches.end());
      return matches;
    }
    auto it = commands_.find(std::string(line.substr(0, split)));
    if (it == commands_.end()) return matches;
    const auto start = line.find_first_not_of(" \t", split);
    const auto prefix = start == std::string_view::npos ? std::string_view{} : line.substr(start);
    for (const auto &option : it->second.completions) {
      if (!option.starts_with(prefix)) continue;
      std::ostringstream completed;
      completed << it->first << ' ' << std::quoted(option);
      matches.push_back(completed.str());
    }
    std::sort(matches.begin(), matches.end());
    matches.erase(std::unique(matches.begin(), matches.end()), matches.end());
    return matches;
  }

  void previous() {
    const auto &history = history_;
    if (history.empty()) return;
    if (!distance_) draft_ = input;
    distance_ = std::min(distance_ + 1, history.size());
    input = history[history.size() - distance_];
  }

  void next() {
    if (!distance_) return;
    --distance_;
    const auto &history = history_;
    input = distance_ && distance_ <= history.size() ? history[history.size() - distance_] : draft_;
  }

  void submit() {
    execute(input);
    input.clear();
    draft_.clear();
    distance_ = 0;
  }

  void complete_input() {
    const auto matches = complete(input);
    if (matches.empty()) return;
    if (matches.size() == 1) {
      input = matches.front() + ' ';
      return;
    }
    for (const auto &match : matches) print({match});
  }

 private:
  Result run(Arguments words) {
    const auto &name = words.front();
    const auto args = words.subspan(1);
    if (name == "help") return help(args);
    if (name == "clear") {
      if (!args.empty()) return {"Usage: clear", false};
      clear();
      return {};
    }
    const auto it = commands_.find(name);
    if (it == commands_.end()) return {"Unknown command: " + name + ". Type help.", false};
    auto callback = it->second.run;
    return callback(args);
  }

  Result help(Arguments args) const {
    if (args.size() > 1) return {"Usage: help [command]", false};
    if (!args.empty()) {
      if (args[0] == "help") return {"help [command] - List commands or show help"};
      if (args[0] == "clear") return {"clear - Clear output"};
      const auto it = commands_.find(args[0]);
      if (it == commands_.end()) return {"Unknown command: " + args[0], false};
      return {it->first + " - " + it->second.help};
    }
    std::string text = "clear - Clear output\nhelp [command] - List commands or show help";
    for (const auto &[name, command] : commands_) text += "\n" + name + " - " + command.help;
    return {std::move(text)};
  }

  std::map<std::string, Command> commands_;
  std::deque<Result> output_;
  std::deque<std::string> history_;
  size_t output_limit_;
  size_t history_limit_;
  size_t output_revision_ = 0;
  size_t distance_ = 0;
  std::string draft_;
};

}
