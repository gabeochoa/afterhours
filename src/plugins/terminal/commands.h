#pragma once

#include "arguments.h"
#include <cctype>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace afterhours::terminal {

struct Result {
  std::string text;
  bool success = true;
};

struct CommandBase {
  virtual ~CommandBase() = default;
  virtual std::string_view name() const = 0;
  virtual std::string_view help() const = 0;
  virtual Result run(Arguments args) = 0;
  virtual std::vector<std::string> completions() const { return {}; }
};

struct Command {
  std::string name;
  std::string help;
  std::function<Result(Arguments)> run;
  std::vector<std::string> completions = {};
};

namespace detail {

struct ParsedLine {
  std::vector<std::string> words;
  std::string error;
};

inline ParsedLine parse(std::string_view line) {
  ParsedLine result;
  std::string word;
  char quote = 0;
  bool started = false;
  for (size_t i = 0; i < line.size(); ++i) {
    const char c = line[i];
    if (c == '\\' && i + 1 < line.size() &&
        (line[i + 1] == '\\' || line[i + 1] == '"' || line[i + 1] == '\'')) {
      word += line[++i];
      started = true;
      continue;
    }
    if (quote) {
      if (c == quote) quote = 0;
      else word += c;
      continue;
    }
    if (c == '"' || c == '\'') {
      quote = c;
      started = true;
      continue;
    }
    if (std::isspace(static_cast<unsigned char>(c))) {
      if (started) result.words.push_back(std::move(word));
      word.clear();
      started = false;
      continue;
    }
    started = true;
    word += c;
  }
  if (quote) return {{}, "Unclosed quote"};
  if (started) result.words.push_back(std::move(word));
  return result;
}

}

}
