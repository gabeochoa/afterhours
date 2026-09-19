#pragma once

#include "arguments.h"
#include <cctype>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace afterhours::terminal {

struct Result {
  std::string text;
  bool success = true;
};

struct CompletionRequest {
  Arguments arguments;
  size_t argument_index;
  std::string_view prefix;
  std::string_view line;
};

struct CommandBase {
  virtual ~CommandBase() = default;
  virtual std::string_view name() const = 0;
  virtual std::string_view help() const = 0;
  virtual Result run(Arguments args) = 0;
  virtual std::string_view usage() const { return {}; }
  virtual std::optional<std::string> unavailable_reason() const { return {}; }
  virtual std::vector<std::string> completions() const { return {}; }
  virtual std::vector<std::string> complete(const CompletionRequest &request) const {
    if (request.argument_index != 0) return {};
    return completions();
  }
};

struct Command {
  std::string name;
  std::string help;
  std::function<Result(Arguments)> run;
  std::vector<std::string> completions = {};
  std::function<std::vector<std::string>(const CompletionRequest &)> complete = {};
  std::string usage = {};
  std::function<std::optional<std::string>()> unavailable_reason = {};
};

namespace detail {

struct ParsedLine {
  std::vector<std::string> words;
  std::string error;
};

inline ParsedLine parse(std::string_view line, bool allow_unclosed = false) {
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
  if (quote && !allow_unclosed) return {{}, "Unclosed quote"};
  if (started) result.words.push_back(std::move(word));
  return result;
}

}

}
