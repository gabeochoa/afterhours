#pragma once

#include <mutex>
#include <vector>

namespace afterhours::window_events {

// Window lifecycle the frame loop used to absorb silently: a resize only
// changed internal projection state, an iconify/restore or suspend/resume
// never reached the app, and a backing-scale (DPI) move between displays
// changed sapp_dpi_scale()/GetWindowScaleDPI() with no signal, so a retained
// or on-demand app could not know its last frame was stale or mis-scaled.
enum class Type {
  Resized,       // logical size changed; width/height/scale are current
  ScaleChanged,  // backing scale changed (may accompany Resized)
  Occluded,      // iconified/minimized/hidden — not drawable
  Revealed,      // restored/deiconified — drawable again
  Suspended,     // OS suspended the app (mobile/web lifecycle)
  Resumed,
};

struct Event {
  Type type;
  int width = 0;    // logical window width at the event, 0 if unknown
  int height = 0;
  float scale = 0.f; // backing (DPI) scale at the event, 0 if unknown
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
} // namespace detail

// Called by backends (and tests injecting synthetic events). Coalescing is
// the caller's job: backends only push on actual transitions.
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

// Drain every pending event. Empty when nothing happened — no spurious fire.
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

// Last backing scale a backend reported, 0 until the first event.
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

} // namespace afterhours::window_events
