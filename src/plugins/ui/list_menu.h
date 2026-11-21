#pragma once

// List-menu navigation for a menu the app draws itself (Pumpkin K).
//
//   list_menu::State nav;
//   auto r = list_menu::update_list_menu(nav, (int)rows.size());
//   if (r.activated) open(rows[nav.selected]);
//   // draw each row yourself, highlighting nav.selected
//
// Caller owns the rows and the drawing; this owns only the selection
// index. Feed it ListMenuInput directly in tests, or use the overload
// with no input argument to read this frame's keys (Up/Down with repeat,
// PageUp/PageDown, Home/End, Enter/Space to activate).

#include <functional>

#include "../../core/key_codes.h"
#include "../input_system.h"

namespace afterhours {
namespace list_menu {

struct State {
  int selected = -1; // -1: nothing selected
  bool wrap = true;  // false: clamp at the first/last enabled row
  int page_size = 10;
};

struct Input {
  bool up = false;
  bool down = false;
  bool page_up = false;
  bool page_down = false;
  bool home = false;
  bool end = false;
  bool activate = false;
};

struct Result {
  bool moved = false;
  bool activated = false;
};

// Null predicate means every row is enabled.
using EnabledFn = std::function<bool(int)>;

inline bool row_enabled(const EnabledFn &enabled, int i) {
  return !enabled || enabled(i);
}

inline int first_enabled(int count, const EnabledFn &enabled) {
  for (int i = 0; i < count; i++)
    if (row_enabled(enabled, i)) return i;
  return -1;
}

inline int last_enabled(int count, const EnabledFn &enabled) {
  for (int i = count - 1; i >= 0; i--)
    if (row_enabled(enabled, i)) return i;
  return -1;
}

// One enabled step from `from`; stays put at an end unless wrapping.
inline int step(int from, int count, int dir, bool wrap,
                const EnabledFn &enabled) {
  if (count <= 0) return -1;
  if (from < 0) return dir < 0 ? last_enabled(count, enabled)
                               : first_enabled(count, enabled);
  for (int n = 1; n <= count; n++) {
    int i = from + dir * n;
    if (wrap) {
      i = ((i % count) + count) % count;
    } else if (i < 0 || i >= count) {
      return from;
    }
    if (row_enabled(enabled, i)) return i;
    if (wrap && i == from) break; // sole enabled row is the current one
  }
  return from;
}

inline Result update_list_menu(State &s, int count, const Input &in,
                               const EnabledFn &enabled = nullptr) {
  Result r;
  if (count <= 0) {
    s.selected = -1;
    return r;
  }
  // Drop a selection that fell off the list or onto a disabled row.
  if (s.selected >= count ||
      (s.selected >= 0 && !row_enabled(enabled, s.selected)))
    s.selected = first_enabled(count, enabled);
  if (s.selected < 0) s.selected = first_enabled(count, enabled);

  const int before = s.selected;
  if (in.home) s.selected = first_enabled(count, enabled);
  if (in.end) s.selected = last_enabled(count, enabled);
  if (in.up) s.selected = step(s.selected, count, -1, s.wrap, enabled);
  if (in.down) s.selected = step(s.selected, count, 1, s.wrap, enabled);
  const int page = s.page_size > 0 ? s.page_size : 1;
  for (int i = 0; in.page_up && i < page; i++) {
    int next = step(s.selected, count, -1, s.wrap, enabled);
    if (next == s.selected) break;
    s.selected = next;
  }
  for (int i = 0; in.page_down && i < page; i++) {
    int next = step(s.selected, count, 1, s.wrap, enabled);
    if (next == s.selected) break;
    s.selected = next;
  }
  r.moved = s.selected != before;
  r.activated = in.activate && s.selected >= 0 &&
                row_enabled(enabled, s.selected);
  return r;
}

inline Input input_from_keys() {
  Input in;
  auto pressed_repeat = [](int key) {
    return input::is_key_pressed(key) || input::is_key_pressed_repeat(key);
  };
  in.up = pressed_repeat(keys::UP);
  in.down = pressed_repeat(keys::DOWN);
  in.page_up = pressed_repeat(keys::PAGE_UP);
  in.page_down = pressed_repeat(keys::PAGE_DOWN);
  in.home = input::is_key_pressed(keys::HOME);
  in.end = input::is_key_pressed(keys::END);
  in.activate = input::is_key_pressed(keys::ENTER) ||
                input::is_key_pressed(keys::SPACE) ||
                input::is_key_pressed(keys::KP_ENTER);
  return in;
}

inline Result update_list_menu(State &s, int count,
                               const EnabledFn &enabled = nullptr) {
  return update_list_menu(s, count, input_from_keys(), enabled);
}

} // namespace list_menu
} // namespace afterhours
