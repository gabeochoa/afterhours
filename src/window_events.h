#pragma once

#include <mutex>
#include <vector>

namespace afterhours::window_events {

enum class Type {
  Resized,
  ScaleChanged,
  Occluded,
  Revealed,
  Suspended,
  Resumed,
};

struct Event {
  Type type;
  int width = 0;
  int height = 0;
  float scale = 0.f;
};

namespace detail {
struct State {
  std::mutex mutex;
  std::vector<Event> pending;
  bool occluded = false;
  float scale = 0.f;
};
inline State &state() {
  static State s;
  return s;
}
}

inline void push(Event e) {
  auto &s = detail::state();
  std::lock_guard lock(s.mutex);
  if (e.type == Type::Occluded || e.type == Type::Suspended)
    s.occluded = true;
  if (e.type == Type::Revealed || e.type == Type::Resumed)
    s.occluded = false;
  if (e.scale > 0.f)
    s.scale = e.scale;
  s.pending.push_back(e);
}

inline std::vector<Event> take() {
  auto &s = detail::state();
  std::lock_guard lock(s.mutex);
  std::vector<Event> out;
  out.swap(s.pending);
  return out;
}

inline bool occluded() {
  auto &s = detail::state();
  std::lock_guard lock(s.mutex);
  return s.occluded;
}

inline float last_scale() {
  auto &s = detail::state();
  std::lock_guard lock(s.mutex);
  return s.scale;
}

inline void clear() {
  auto &s = detail::state();
  std::lock_guard lock(s.mutex);
  s.pending.clear();
  s.occluded = false;
  s.scale = 0.f;
}

}
