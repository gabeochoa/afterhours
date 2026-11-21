// kv_settings.h — tiny header-only key=value settings, no JSON dependency.
//
// Format: one `key = value` per line. Leading/trailing whitespace on keys and
// values is trimmed, blank lines and lines whose first non-space char is '#'
// are skipped, and a line without '=' is ignored. Values may contain '=' and
// '#'; split is on the first '='. Keys are unique: the last one wins.
#pragma once

#include <charconv>
#include <cctype>
#include <map>
#include <string>
#include <string_view>

#include "files.h"

namespace afterhours {

struct kv_settings {
  std::map<std::string, std::string> entries;

  [[nodiscard]] bool has(const std::string &key) const {
    return entries.contains(key);
  }

  [[nodiscard]] std::string get(const std::string &key,
                                const std::string &fallback = {}) const {
    const auto it = entries.find(key);
    return it == entries.end() ? fallback : it->second;
  }

  [[nodiscard]] int get_int(const std::string &key, int fallback) const {
    const std::string v = get(key);
    int out = 0;
    const auto [p, ec] = std::from_chars(v.data(), v.data() + v.size(), out);
    return ec == std::errc{} && p == v.data() + v.size() ? out : fallback;
  }

  [[nodiscard]] double get_double(const std::string &key,
                                  double fallback) const {
    const std::string v = get(key);
    if (v.empty())
      return fallback;
    char *end = nullptr;
    const double out = std::strtod(v.c_str(), &end);
    return end == v.c_str() + v.size() ? out : fallback;
  }

  [[nodiscard]] bool get_bool(const std::string &key, bool fallback) const {
    std::string v = get(key);
    for (char &c : v)
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (v == "true" || v == "1" || v == "yes" || v == "on")
      return true;
    if (v == "false" || v == "0" || v == "no" || v == "off")
      return false;
    return fallback;
  }

  void set(const std::string &key, std::string value) {
    entries[key] = std::move(value);
  }

  bool erase(const std::string &key) { return entries.erase(key) > 0; }

  [[nodiscard]] static std::string serialize(
      const std::map<std::string, std::string> &map) {
    std::string out;
    for (const auto &[k, v] : map) {
      out += k;
      out += " = ";
      out += v;
      out += '\n';
    }
    return out;
  }

  static void parse_into(std::string_view text,
                         std::map<std::string, std::string> &map) {
    size_t pos = 0;
    while (pos <= text.size()) {
      const size_t nl = text.find('\n', pos);
      std::string_view line =
          text.substr(pos, nl == std::string_view::npos ? std::string_view::npos
                                                         : nl - pos);
      pos = nl == std::string_view::npos ? text.size() + 1 : nl + 1;
      if (!line.empty() && line.back() == '\r')
        line.remove_suffix(1);
      const size_t begin = line.find_first_not_of(" \t");
      if (begin == std::string_view::npos)
        continue; // blank line
      line = line.substr(begin);
      if (line.front() == '#')
        continue; // comment
      const size_t eq = line.find('=');
      if (eq == std::string_view::npos)
        continue; // corrupt line: not a key=value pair
      const std::string_view key = line.substr(0, eq);
      std::string_view value = line.substr(eq + 1);
      const size_t key_end = key.find_last_not_of(" \t");
      const size_t value_begin = value.find_first_not_of(" \t");
      const size_t value_end = value.find_last_not_of(" \t");
      if (key_end == std::string_view::npos)
        continue; // empty key
      map[std::string(key.substr(0, key_end + 1))] =
          value_begin == std::string_view::npos
              ? std::string{}
              : std::string(
                    value.substr(value_begin, value_end - value_begin + 1));
    }
  }

  // Returns {} for a missing/unreadable file; corrupt lines are skipped.
  static kv_settings load(const std::filesystem::path &path) {
    kv_settings s;
    if (const auto text = files::read_string(path))
      parse_into(*text, s.entries);
    return s;
  }

  bool save(const std::filesystem::path &path) const {
    return files::write_string_atomic(path, serialize(entries));
  }
};

} // namespace afterhours
