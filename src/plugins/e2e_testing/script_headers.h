#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace afterhours {
namespace testing {

struct SpawnSpec {
    std::string label;
    std::vector<std::string> argv;
};

struct WaitForLogSpec {
    std::string label;
    std::string needle;
};

struct ScriptHeaders {
    std::optional<float> timeout_seconds;
    std::vector<std::string> flags;
    std::vector<std::string> requirements;
    std::string batch;
    std::vector<SpawnSpec> spawns;
    std::vector<WaitForLogSpec> wait_for_logs;
    std::vector<std::string> errors;
};
inline std::vector<std::string> split_shell_words(const std::string &text,
                                                  std::string *error = nullptr) {
    std::vector<std::string> words;
    std::string current;
    bool in_single = false, in_double = false, escaped = false, has = false;
    for (const char ch : text) {
        if (escaped) {
            current += ch;
            escaped = false;
            has = true;
        } else if (ch == '\\' && !in_single) {
            escaped = true;
        } else if (ch == '\'' && !in_double) {
            in_single = !in_single;
            has = true;
        } else if (ch == '"' && !in_single) {
            in_double = !in_double;
            has = true;
        } else if (std::isspace(static_cast<unsigned char>(ch)) && !in_single &&
                   !in_double) {
            if (has) {
                words.push_back(current);
                current.clear();
                has = false;
            }
        } else {
            current += ch;
            has = true;
        }
    }
    if (error && (in_single || in_double))
        *error = "unterminated quote";
    if (has) words.push_back(current);
    return words;
}

namespace detail {

inline std::string trim_copy(const std::string &text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

inline std::vector<std::string> split_requirements(const std::string &text) {
    std::vector<std::string> out;
    std::string current;
    for (const char ch : text) {
        if (ch == ',' || std::isspace(static_cast<unsigned char>(ch))) {
            if (!current.empty()) {
                out.push_back(current);
                current.clear();
            }
        } else {
            current += ch;
        }
    }
    if (!current.empty()) out.push_back(current);
    return out;
}

inline std::optional<float> parse_timeout_seconds(const std::string &text) {
    std::string value = trim_copy(text);
    if (!value.empty() && (value.back() == 's' || value.back() == 'S'))
        value.pop_back();
    try {
        std::size_t used = 0;
        const float seconds = std::stof(value, &used);
        if (used != value.size() || seconds <= 0.f) return std::nullopt;
        return seconds;
    } catch (...) {
        return std::nullopt;
    }
}

}
inline ScriptHeaders parse_script_headers_text(const std::string &text) {
    ScriptHeaders headers;
    std::istringstream input(text);
    std::string line;
    int line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const std::string trimmed = detail::trim_copy(line);
        if (trimmed.empty()) continue;
        if (trimmed[0] != '#') break;

        const std::string body = detail::trim_copy(trimmed.substr(1));
        const auto colon = body.find(':');
        if (colon == std::string::npos) continue;
        const std::string key = detail::trim_copy(body.substr(0, colon));
        const std::string value = detail::trim_copy(body.substr(colon + 1));
        const std::string where =
            "line " + std::to_string(line_number) + ": # " + key + ":";

        if (key == "timeout") {
            if (const auto seconds = detail::parse_timeout_seconds(value))
                headers.timeout_seconds = *seconds;
            else
                headers.errors.push_back(where + " expected seconds > 0, got '" +
                                         value + "'");
        } else if (key == "flags") {
            std::string error;
            auto words = split_shell_words(value, &error);
            if (!error.empty())
                headers.errors.push_back(where + " " + error);
            else
                headers.flags.insert(headers.flags.end(), words.begin(), words.end());
        } else if (key == "requires") {
            auto reqs = detail::split_requirements(value);
            headers.requirements.insert(headers.requirements.end(), reqs.begin(), reqs.end());
        } else if (key == "batch") {
            headers.batch = value;
        } else if (key == "spawn") {
            std::string error;
            auto words = split_shell_words(value, &error);
            if (!error.empty()) {
                headers.errors.push_back(where + " " + error);
            } else if (words.size() < 2) {
                headers.errors.push_back(where +
                                         " expected '<label> <command...>'");
            } else {
                SpawnSpec spec;
                spec.label = words[0];
                spec.argv.assign(words.begin() + 1, words.end());
                headers.spawns.push_back(std::move(spec));
            }
        } else if (key == "wait_for_log") {
            WaitForLogSpec spec;
            const auto sep = value.find(':');
            const std::string left =
                sep == std::string::npos ? "" : value.substr(0, sep);
            if (sep != std::string::npos && !left.empty() &&
                left.find_first_of(" \t") == std::string::npos) {
                spec.label = left;
                spec.needle = detail::trim_copy(value.substr(sep + 1));
            } else {
                spec.needle = value;
            }
            if (spec.needle.empty())
                headers.errors.push_back(where + " expected a log needle");
            else
                headers.wait_for_logs.push_back(std::move(spec));
        }
    }
    return headers;
}

inline ScriptHeaders parse_script_headers(const std::string &path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        ScriptHeaders headers;
        headers.errors.push_back("cannot read script: " + path);
        return headers;
    }
    std::ostringstream text;
    text << file.rdbuf();
    return parse_script_headers_text(text.str());
}
inline std::string effective_batch(const ScriptHeaders &headers,
                                   const std::string &script_name) {
    return headers.batch.empty() ? script_name : headers.batch;
}
inline std::vector<std::vector<std::size_t>> group_by_batch(
    const std::vector<ScriptHeaders> &headers,
    const std::vector<std::string> &names) {
    std::vector<std::vector<std::size_t>> groups;
    std::vector<std::string> keys;
    for (std::size_t i = 0; i < headers.size(); ++i) {
        const std::string name = i < names.size() ? names[i] : std::to_string(i);
        const std::string key = effective_batch(headers[i], name);
        const auto it = std::find(keys.begin(), keys.end(), key);
        if (it == keys.end()) {
            keys.push_back(key);
            groups.push_back({i});
        } else {
            groups[static_cast<std::size_t>(it - keys.begin())].push_back(i);
        }
    }
    return groups;
}

}
}
