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
  bool enter_accepts_first_suggestion = true;

  explicit Console(size_t output_limit = 200, size_t history_limit = 100)
      : output_limit_(output_limit), history_limit_(history_limit) {}

  bool add_command(Command command) {
    if (command.name.empty() || !command.run || command.name == "help" ||
        command.name == "clear") return false;
    for (unsigned char c : command.name)
      if (!std::isalnum(c) && c != '_' && c != '-' && c != '.') return false;
    auto name = command.name;
    const bool added = commands_.emplace(std::move(name), std::move(command)).second;
    if (added) ++command_revision_;
    return added;
  }

  bool add_command(std::unique_ptr<CommandBase> command) {
    if (!command) return false;
    auto owned = std::shared_ptr<CommandBase>(std::move(command));
    return add_command({std::string(owned->name()), std::string(owned->help()),
                        [owned](Arguments args) { return owned->run(args); },
                        {}, [owned](const CompletionRequest &request) {
                          return owned->complete(request);
                        }});
  }

  bool remove_command(const std::string &name) {
    if (!commands_.erase(name)) return false;
    ++command_revision_;
    return true;
  }
  size_t command_revision() const { return command_revision_; }
  void invalidate_completions() { ++command_revision_; }
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

  std::string_view command_help(std::string_view name) const {
    if (name == "help") return "List commands or show help";
    if (name == "clear") return "Clear output";
    auto it = commands_.find(std::string(name));
    if (it == commands_.end()) return {};
    return it->second.help;
  }

  std::vector<std::string> complete(std::string_view line) const {
    const auto parsed = detail::parse(line, true);
    const bool trailing_space = !line.empty() &&
        std::isspace(static_cast<unsigned char>(line.back())) &&
        detail::parse(line).error.empty();
    std::vector<std::string> matches;
    const auto &words = parsed.words;
    if (words.empty() || (words.size() == 1 && !trailing_space)) {
      const std::string_view prefix = words.empty() ? std::string_view{} : words[0];
      for (const auto &name : {"clear", "help"})
        if (std::string_view(name).starts_with(prefix)) matches.emplace_back(name);
      for (const auto &[name, command] : commands_)
        if (name.starts_with(prefix)) matches.push_back(name);
      std::sort(matches.begin(), matches.end());
      return matches;
    }
    const size_t argument_index = words.size() - (trailing_space ? 1 : 2);
    const std::string_view prefix = trailing_space ? std::string_view{} : words.back();
    if (words[0] == "help") {
      if (argument_index != 0) return matches;
      for (const auto &name : {"clear", "help"})
        if (std::string_view(name).starts_with(prefix)) matches.push_back(std::string("help ") + name);
      for (const auto &[name, command] : commands_)
        if (name.starts_with(prefix)) matches.push_back("help " + name);
      std::sort(matches.begin(), matches.end());
      return matches;
    }
    auto it = commands_.find(words[0]);
    if (it == commands_.end()) return matches;
    const CompletionRequest request{Arguments(words).subspan(1), argument_index, prefix, line};
    const auto provider = it->second.complete;
    auto options = provider ? provider(request) :
        (argument_index == 0 ? it->second.completions : std::vector<std::string>{});
    std::sort(options.begin(), options.end());
    options.erase(std::unique(options.begin(), options.end()), options.end());
    auto append_argument = [](std::ostream &out, const std::string &word) {
      out << ' ';
      const bool needs_quotes = word.empty() || std::any_of(word.begin(), word.end(), [](unsigned char c) {
        return std::isspace(c) || c == '"' || c == '\'' || c == '\\';
      });
      if (needs_quotes) {
        out << std::quoted(word);
        return;
      }
      out << word;
    };
    for (const auto &option : options) {
      if (!option.starts_with(prefix)) continue;
      std::ostringstream completed;
      completed << words[0];
      for (size_t i = 0; i < argument_index; ++i) append_argument(completed, words[i + 1]);
      append_argument(completed, option);
      matches.push_back(completed.str());
    }
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
  size_t command_revision_ = 0;
  size_t distance_ = 0;
  std::string draft_;
};

}
