#pragma once

#include <mutex>
#include <string>
#include <vector>

namespace afterhours::file_drop {

// OS file drag-and-drop, the same poll shape as the other window surfaces:
// opt in with RunConfig::enable_file_drop, then drain paths once per frame.
// Disabled means the backend never enables the OS drop target and nothing
// is queued — there is no fake delivery.

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
} // namespace detail

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

// Called by backends on a real OS drop (and by tests injecting one). The
// whole drop is queued at once so take() preserves the drop's path order.
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

// Drain every queued dropped-file path, oldest drop first. Empty when
// nothing was dropped (or dropping was never enabled).
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

} // namespace afterhours::file_drop
