#pragma once
#include <cctype>
#include <cstdlib>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>
#include "logging.h"
namespace afterhours {
namespace developer {
struct tunable_reload_result {
  std::size_t applied = 0;
  std::size_t changed = 0;
  std::vector<std::string> errors;
};
namespace tunable_detail {
inline std::string_view trim(std::string_view s) {
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
  return s;
}
inline std::string lower(std::string_view s) {
  std::string out;
  for (char c : s) out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return out;
}
template <typename T> bool parse_value(std::string_view text, T &out);
template <> inline bool parse_value<bool>(std::string_view text, bool &out) {
  const std::string v = lower(trim(text));
  if (v == "true" || v == "1" || v == "on" || v == "yes") { out = true; return true; }
  if (v == "false" || v == "0" || v == "off" || v == "no") { out = false; return true; }
  return false;
}
template <> inline bool parse_value<std::string>(std::string_view text, std::string &out) {
  out = std::string(trim(text));
  return true;
}
template <typename T> requires (std::is_integral_v<T> && !std::is_same_v<T, bool>)
bool parse_value(std::string_view text, T &out) {
  const std::string s(trim(text));
  if (s.empty()) return false;
  char *end = nullptr;
  const long long v = std::strtoll(s.c_str(), &end, 10);
  if (end != s.c_str() + s.size()) return false;
  out = static_cast<T>(v);
  return static_cast<long long>(out) == v;
}
template <typename T> requires std::is_floating_point_v<T>
bool parse_value(std::string_view text, T &out) {
  const std::string s(trim(text));
  if (s.empty()) return false;
  char *end = nullptr;
  const double v = std::strtod(s.c_str(), &end);
  if (end != s.c_str() + s.size()) return false;
  out = static_cast<T>(v);
  return true;
}
struct entry {
  virtual ~entry() = default;
  virtual bool apply(std::string_view text, std::string &error, bool &changed_now) = 0;
};
}
class tunable_registry {
 public:
  bool add(const std::string &name, tunable_detail::entry *e) {
    if (entries_.count(name)) { log_error("tunable: duplicate name '{}' ignored", name); return false; }
    entries_[name] = e;
    return true;
  }
  void remove(const std::string &name, tunable_detail::entry *e) {
    auto it = entries_.find(name);
    if (it != entries_.end() && it->second == e) entries_.erase(it);
  }
  std::size_t size() const { return entries_.size(); }
  bool contains(const std::string &name) const { return entries_.count(name) > 0; }
  bool set(const std::string &name, std::string_view text, std::string *error = nullptr) {
    auto it = entries_.find(name);
    if (it == entries_.end()) {
      if (error) *error = "unknown tunable: " + name;
      return false;
    }
    std::string why;
    bool changed_now = false;
    if (!it->second->apply(text, why, changed_now)) {
      if (error) *error = why;
      return false;
    }
    return true;
  }
  tunable_reload_result reload(std::string_view source) {
    tunable_reload_result result;
    std::size_t pos = 0, line_no = 0;
    while (pos <= source.size()) {
      const std::size_t nl = source.find('\n', pos);
      std::string_view line = source.substr(pos, nl == std::string_view::npos ? std::string_view::npos : nl - pos);
      ++line_no;
      const std::string_view t = tunable_detail::trim(line);
      if (!t.empty() && t.front() != '#' && !(t.size() > 1 && t[0] == '/' && t[1] == '/')) {
        const std::size_t eq = t.find('=');
        const std::string where = "line " + std::to_string(line_no) + ": ";
        if (eq == std::string_view::npos) {
          result.errors.push_back(where + "expected 'name = value'");
        } else {
          const std::string name(tunable_detail::trim(t.substr(0, eq)));
          auto it = entries_.find(name);
          std::string why;
          bool changed_now = false;
          if (it == entries_.end()) result.errors.push_back(where + "unknown tunable: " + name);
          else if (!it->second->apply(t.substr(eq + 1), why, changed_now)) result.errors.push_back(where + why);
          else { ++result.applied; if (changed_now) ++result.changed; }
        }
      }
      if (nl == std::string_view::npos) break;
      pos = nl + 1;
    }
    return result;
  }
 private:
  std::map<std::string, tunable_detail::entry *> entries_;
};
inline tunable_registry &tunable_registry_instance() {
  static tunable_registry instance;
  return instance;
}
inline tunable_reload_result reload_tunables(std::string_view source) {
  return tunable_registry_instance().reload(source);
}
inline bool set_tunable(const std::string &name, std::string_view text, std::string *error = nullptr) {
  return tunable_registry_instance().set(name, text, error);
}
template <typename T> class tunable : public tunable_detail::entry {
 public:
  tunable(std::string name, T initial) : name_(std::move(name)), value_(std::move(initial)) {
    registered_ = tunable_registry_instance().add(name_, this);
  }
  ~tunable() override { if (registered_) tunable_registry_instance().remove(name_, this); }
  tunable(const tunable &) = delete;
  tunable &operator=(const tunable &) = delete;
  const std::string &name() const { return name_; }
  const T &get() const { return value_; }
  operator const T &() const { return value_; }
  void on_change(std::function<void(const T &)> callback) { callback_ = std::move(callback); }
  void set(const T &v) {
    if (v == value_) return;
    value_ = v;
    if (callback_) callback_(value_);
  }
 private:
  bool apply(std::string_view text, std::string &error, bool &changed_now) override {
    T parsed{};
    if (!tunable_detail::parse_value<T>(text, parsed)) {
      error = "bad value '" + std::string(tunable_detail::trim(text)) + "' for " + name_;
      return false;
    }
    changed_now = !(parsed == value_);
    set(parsed);
    return true;
  }
  std::string name_;
  T value_;
  bool registered_ = false;
  std::function<void(const T &)> callback_;
};
} // namespace developer
} // namespace afterhours
