#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "developer.h"

namespace afterhours {

// What the app drew, on whatever backend it drew with.
//
// The `none` backend has always recorded its draws, which is how the library
// tests assert on rendering rather than on layout. An app builds against
// raylib or sokol, so none of that was reachable from an app-level test: the
// only readback was a PNG, and answering "was this border painted" meant
// profiling pixel columns and hoping you picked the right ones.
//
// Recording lives here rather than in a backend so every backend shares one
// buffer and one answer. Backends call record() from the ops the UI render
// path emits; it returns immediately unless a caller asked for capture, so a
// shipping build pays one predictable branch per draw.
namespace capture {

struct DrawnCall {
  std::string op;
  RectangleType rect{};
  ColorType color{};
  std::string text;
  // Which widget drew it, and on which layer. -1 when the draw happened
  // outside any widget, which is honest rather than blaming whoever ran last.
  int entity_id = -1;
  int layer = 0;
};

inline int &current_entity() {
  static int id = -1;
  return id;
}

inline int &current_layer() {
  static int layer = 0;
  return layer;
}

// Attributes every draw made while it is alive to one widget. Restores on
// exit, including on the early returns the render path is full of.
struct Scope {
  int prev_entity;
  int prev_layer;
  Scope(int entity_id, int layer)
      : prev_entity(current_entity()), prev_layer(current_layer()) {
    current_entity() = entity_id;
    current_layer() = layer;
  }
  ~Scope() {
    current_entity() = prev_entity;
    current_layer() = prev_layer;
  }
};

inline bool &enabled() {
  static bool on = false;
  return on;
}

inline std::vector<DrawnCall> &calls() {
  static std::vector<DrawnCall> buf;
  return buf;
}

inline void enable() { enabled() = true; }
inline void disable() { enabled() = false; }
inline void clear() { calls().clear(); }

// Who decides when a frame starts.
//
// The UI render pass used to clear the buffer at its own start, which threw
// away everything drawn before it -- for a game, the entire world. An app that
// calls begin_frame() takes that over, so world draws survive to be asserted
// on. One that never calls it keeps the old behaviour.
inline bool &app_owns_frame() {
  static bool owned = false;
  return owned;
}

inline void begin_frame() {
  app_owns_frame() = true;
  clear();
}

// A view, not a `const std::string &`: that made every `const char *` caller
// build a string before this could return early.
inline void record(const char *op, const RectangleType &rect,
                   const ColorType &color, std::string_view text = {}) {
  if (!enabled())
    return;
  calls().push_back({op, rect, color, std::string(text), current_entity(),
                     current_layer()});
}

} // namespace capture

} // namespace afterhours
