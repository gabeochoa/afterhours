// E2E script headers -- the per-script declarations a multi-process
// runner reads before it spawns anything.
//
// Pumpkin Beachball's online suite (1,199 lines of hand-rolled Python)
// and every other networked consumer re-implement the same thing:
// spawn helper processes, allocate a free port, barrier on a log line,
// and harvest a failure tail. Cartographer asked for the declarations
// to live in the script itself. This header is that declaration format,
// shared by the shipped runner in tools/e2e_parallel_runner.py and by
// any in-process consumer that wants to schedule scripts itself.
//
// Headers are comment lines in the leading comment block of a .e2e
// script, before the first command. A `#` line after the first command
// is an ordinary comment and is never a header. Unknown headers are
// ignored, so a script carrying headers still runs under the plain
// single-process E2ERunner, which simply never reads them.
//
//   # timeout: 30            wall-clock seconds for this script (30, 30s)
//   # flags: --headless --seed=1
//                            extra argv for the target, shell-split
//   # requires: gpu, file:assets.dat, env:STEAM_SDK, cmd:ffmpeg
//                            unmet requirements skip (not fail) the script
//   # batch: gpu-heavy       scripts sharing a batch run one at a time,
//                            in file order; different batches may overlap
//   # spawn: server ./game --serve --port={port}
//                            helper process started before the target;
//                            {port} {tmp} {script} {binary} expand
//   # wait_for_log: server:READY
//                            do not start the target until the named
//                            spawned process prints the needle; with no
//                            label, the first spawned process is meant
//
// This header is std-only on purpose: schedulers and tools include it
// without pulling in the ECS or a backend.
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
    // Argv after shell-splitting, with runner placeholders ({port} and
    // friends) still unexpanded: the runner owns the values.
    std::vector<std::string> argv;
    std::string raw;
};

struct WaitForLogSpec {
    std::string label;  // empty: the first spawned process
    std::string needle;
};

struct ScriptHeaders {
    std::optional<float> timeout_seconds;
    std::vector<std::string> flags;
    std::vector<std::string> requirements;
    std::string batch;  // empty: the script is its own batch
    std::vector<SpawnSpec> spawns;
    std::vector<WaitForLogSpec> wait_for_logs;
    // Malformed header lines, spelled for the runner to report. A script
    // with errors still parses; the runner fails it before spawning.
    std::vector<std::string> errors;
};

// Shell-like splitting: whitespace separates, single and double quotes
// group, backslash escapes the next character outside single quotes.
// An unterminated quote is reported in `error`, never silently accepted.
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

}  // namespace detail

// Parse the leading header block of script text (the whole file is fine;
// parsing stops at the first non-comment, non-blank line).
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
                spec.raw = value;
                headers.spawns.push_back(std::move(spec));
            }
        } else if (key == "wait_for_log") {
            // `label:needle`, or a bare needle for the first spawn. The
            // needle itself may contain colons, so only a colon whose
            // left side is one spaceless word counts as the separator.
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
        // Unknown headers are deliberately ignored (see file comment).
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

// The batch a script serializes against: its `# batch:` name, or its own
// name when it declares none, so undeclared scripts never serialize
// against each other by accident.
inline std::string effective_batch(const ScriptHeaders &headers,
                                   const std::string &script_name) {
    return headers.batch.empty() ? script_name : headers.batch;
}

// Group script indices into batches. Batches appear in first-seen order
// and keep input order inside, which is the runner's scheduling unit:
// one batch runs sequentially, batches run in parallel up to the job
// limit. `names[i]` pairs with `headers[i]`.
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

}  // namespace testing
}  // namespace afterhours
