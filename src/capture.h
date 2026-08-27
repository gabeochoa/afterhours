#pragma once

#include <string>
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

inline void record(const char *op, const RectangleType &rect, const ColorType &color,
                   const std::string &text = "") {
  if (!enabled())
    return;
  calls().push_back({op, rect, color, text});
}

} // namespace capture

} // namespace afterhours
