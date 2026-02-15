// E2E Testing Framework - High-level Input Queue
// Frame-aware input queue with backend wrapping
#pragma once

#include "input_injector.h"

#include <queue>

namespace afterhours {
namespace testing {
namespace test_input {

struct KeyPress {
  int key = 0;
  bool is_char = false;
  char char_value = 0;
};

namespace detail {
inline std::queue<KeyPress> key_queue;
inline bool test_mode = false;
inline bool key_consumed = false;
inline bool char_consumed = false;
} // namespace detail

// Queue a key press
inline void push_key(int key) {
  KeyPress kp;
  kp.key = key;
  detail::key_queue.push(kp);
}

// Queue a character
inline void push_char(char c) {
  KeyPress kp;
  kp.is_char = true;
  kp.char_value = c;
  detail::key_queue.push(kp);
}

// Clear input queue
inline void clear_queue() {
  while (!detail::key_queue.empty())
    detail::key_queue.pop();
}

// Set mouse position
inline void set_mouse_position(float x, float y) {
  input_injector::set_mouse_position(x, y);
}

// Set mouse position from any HasPosition type
template <HasPosition T> inline void set_mouse_position(const T &pos) {
  set_mouse_position(static_cast<float>(pos.x), static_cast<float>(pos.y));
}

// Simulate mouse press
inline void simulate_mouse_press() {
  auto &m = input_injector::detail::mouse;
  m.left_down = true;
  m.just_pressed = true;
  m.press_frames = 1;
  m.active = true;
}

// Simulate mouse release
inline void simulate_mouse_release() {
  auto &m = input_injector::detail::mouse;
  m.left_down = false;
  m.just_released = true;
  m.active = true;
}

// Click at position (press + release on next frame)
inline void simulate_click(float x, float y) {
  set_mouse_position(x, y);
  simulate_mouse_press();
}

// Reset per-frame state
inline void reset_frame() {
  detail::key_consumed = false;
  detail::char_consumed = false;

  // Save press_frames before injector reset clears flags
  auto &m = input_injector::detail::mouse;
  int pf = m.press_frames;

  // Clears just_pressed/just_released unconditionally
  input_injector::reset_frame();

  // Restore just_pressed if we still have press frames remaining
  // (simulate_mouse_press sets press_frames=1, so just_pressed survives
  // one reset_frame call after the injection frame)
  if (pf > 0) {
    m.press_frames = pf - 1;
    m.just_pressed = true;
  }
}

// Clear all test input state
inline void reset_all() {
  clear_queue();
  input_injector::reset_all();
}

// Convenience helpers (use keys:: constants)
inline void simulate_tab() { push_key(keys::TAB); }
inline void simulate_enter() { push_key(keys::ENTER); }
inline void simulate_escape() { push_key(keys::ESCAPE); }
inline void simulate_backspace() { push_key(keys::BACKSPACE); }
inline void simulate_arrow_left() { push_key(keys::LEFT); }
inline void simulate_arrow_right() { push_key(keys::RIGHT); }
inline void simulate_arrow_up() { push_key(keys::UP); }
inline void simulate_arrow_down() { push_key(keys::DOWN); }

// Check if key pressed (wraps backend call)
template <typename BackendFn>
inline bool is_key_pressed(int key, BackendFn backend_fn) {
  if (input_injector::consume_press(key))
    return true;

  if (detail::test_mode) {
    if (detail::key_queue.empty() || detail::key_consumed) {
      return false;
    }
    if (!detail::key_queue.front().is_char &&
        detail::key_queue.front().key == key) {
      detail::key_queue.pop();
      detail::key_consumed = true;
      return true;
    }
    return false;
  }

  return backend_fn(key);
}

// Check if key is down (wraps backend call)
template <typename BackendFn>
inline bool is_key_down(int key, BackendFn backend_fn) {
  // Check injector for held keys
  if (input_injector::is_key_down(key))
    return true;

  if (detail::test_mode) {
    // In test mode, also check if there's a queued key press for this key
    if (!detail::key_queue.empty() && !detail::key_queue.front().is_char &&
        detail::key_queue.front().key == key) {
      return true;
    }
    return false;
  }

  return backend_fn(key);
}

// Get next character (wraps backend call)
// Note: The queue approach naturally prevents re-reading (chars are popped)
// so we don't use char_consumed flag here - that's for non-queue scenarios
template <typename BackendFn>
inline int get_char_pressed(BackendFn backend_fn) {
  if (detail::test_mode) {
    // Skip non-char entries to find the next character
    while (!detail::key_queue.empty() && !detail::key_queue.front().is_char) {
      detail::key_queue.pop();
    }
    if (detail::key_queue.empty()) {
      return 0;
    }
    if (detail::key_queue.front().is_char) {
      char c = detail::key_queue.front().char_value;
      detail::key_queue.pop();
      return static_cast<int>(c);
    }
    return 0;
  }

  return backend_fn();
}

// Get mouse position (wraps backend call)
template <typename Vec2, typename BackendFn>
inline Vec2 get_mouse_position(BackendFn backend_fn) {
  auto &m = input_injector::detail::mouse;
  if (detail::test_mode && m.active) {
    return Vec2{m.pos.x, m.pos.y};
  }
  return backend_fn();
}

// Check mouse button pressed (wraps backend call)
template <typename BackendFn>
inline bool is_mouse_button_pressed(int button, BackendFn backend_fn) {
  if (detail::test_mode) {
    if (button == 0)
      return input_injector::detail::mouse.just_pressed;
    return false;
  }
  return backend_fn(button);
}

// Check mouse button down (wraps backend call)
template <typename BackendFn>
inline bool is_mouse_button_down(int button, BackendFn backend_fn) {
  if (detail::test_mode) {
    if (button == 0)
      return input_injector::detail::mouse.left_down;
    return false;
  }
  return backend_fn(button);
}

} // namespace test_input
} // namespace testing
} // namespace afterhours
