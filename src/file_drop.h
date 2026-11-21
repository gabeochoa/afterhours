#pragma once

#include <mutex>
#include <string>
#include <vector>

namespace afterhours::file_drop {


namespace detail {
struct State {
  std::mutex mutex;
  std::vector<std::string> pending;
  bool enabled = false;
};
inline State &state() {
  static State s;
  return s;
}
}

inline void set_enabled(bool enabled) {
  auto &s = detail::state();
  std::lock_guard lock(s.mutex);
  s.enabled = enabled;
  if (!enabled)
    s.pending.clear();
}

inline bool enabled() {
  auto &s = detail::state();
  std::lock_guard lock(s.mutex);
  return s.enabled;
}

inline void push_dropped_files(std::vector<std::string> paths) {
  if (paths.empty())
    return;
  auto &s = detail::state();
  std::lock_guard lock(s.mutex);
  if (!s.enabled)
    return;
  for (auto &p : paths)
    s.pending.push_back(std::move(p));
}

inline std::vector<std::string> take_dropped_files() {
  auto &s = detail::state();
  std::lock_guard lock(s.mutex);
  std::vector<std::string> out;
  out.swap(s.pending);
  return out;
}

inline void clear() {
  auto &s = detail::state();
  std::lock_guard lock(s.mutex);
  s.pending.clear();
  s.enabled = false;
}

}
