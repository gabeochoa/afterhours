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

// Press a key for one frame.
//
// Routed through the injector, not the queue: the queue's `key_consumed` latch
// is global and lets exactly ONE caller per frame see a key, while
// InputSystem polls each action three times per gamepad id. So a queued key
// worked in a minimal example and silently vanished in a real app the moment a
// second binding, a hotkey, or a controller existed. consume_press is
// explicitly multi-reader. The queue stays for push_char, where FIFO order is
// the point.
inline void push_key(int key) {
  input_injector::set_key_down(key);
  input_injector::set_key_up(key);
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

// Simulate mouse press (auto-releases after press_frames expires)
inline void simulate_mouse_press() {
  auto &m = input_injector::detail::mouse;
  m.left_down = true;
  m.just_pressed = true;
  m.press_read = false;
  m.press_frames = 1;
  m.auto_release = true;
  m.active = true;
}

// Simulate mouse release
inline void simulate_mouse_release() {
  auto &m = input_injector::detail::mouse;
  m.left_down = false;
  m.just_released = true;
  m.press_frames = 0;
  m.auto_release = false;
  m.active = true;
}

// Click at position (press + release on next frame)
inline void simulate_click(float x, float y) {
  set_mouse_position(x, y);
  simulate_mouse_press();
}

// Secondary click. Held for one frame then released by reset_frame, same as
// the left button, so the UI sees a clean down-then-up transition.
inline void simulate_right_click(float x, float y) {
  set_mouse_position(x, y);
  auto &m = input_injector::detail::mouse;
  m.active = true;
  m.right_down = true;
  m.press_frames = 1;
  m.auto_release = true;
}

// Middle click. Same one-frame hold and auto-release as the other two.
inline void simulate_middle_click(float x, float y) {
  set_mouse_position(x, y);
  auto &m = input_injector::detail::mouse;
  m.active = true;
  m.middle_down = true;
  m.middle_just_pressed = true;
  m.middle_press_read = false;
  m.press_frames = 1;
  m.auto_release = true;
}

// Reset per-frame state
inline void reset_frame() {
  detail::key_consumed = false;
  detail::char_consumed = false;

  // Save press_frames before injector reset clears flags
  auto &m = input_injector::detail::mouse;
  int pf = m.press_frames;
  const bool press_seen = m.press_read;
  const bool middle_press_seen = m.middle_press_read;
  m.press_read = false;
  m.middle_press_read = false;

  // Clears just_pressed/just_released unconditionally
  input_injector::reset_frame();

  // Restore just_pressed if we still have press frames remaining
  // (simulate_mouse_press sets press_frames=1, so just_pressed survives
  // one reset_frame call after the injection frame)
  // Either button holds the press open. Gating this on left_down alone left a
  // right-click held down forever -- press_frames never decremented and
  // right_down never cleared -- so every later script in the run saw a stuck
  // secondary button.
  const bool any_down = m.left_down || m.right_down || m.middle_down;
  if (pf > 0 && any_down) {
    m.press_frames = pf - 1;
    // Only re-raise the edge if nothing read it on the frame just ended. The
    // window exists for readers that run before the injecting command; holding
    // it open past an actual read made one `click` fire a cycling handler
    // twice, and the button stays down either way.
    if (m.left_down && !press_seen)
      m.just_pressed = true;
    if (m.middle_down && !middle_press_seen)
      m.middle_just_pressed = true;
  } else if (m.auto_release && any_down) {
    // Auto-release: simulate_click/simulate_mouse_press set auto_release
    // and press_frames=1. Once press_frames expires, release the button
    // so subsequent clicks see a clean down-transition and produce
    // just_pressed=true in the UI system.
    // just_released is the LEFT button's flag; setting it for a right-only
    // press would fake a left click that never happened.
    if (m.left_down)
      m.just_released = true;
    m.left_down = false;
    m.right_down = false;
    m.middle_down = false;
    m.auto_release = false;
    m.press_frames = 0;
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
    // Do not discard queued key entries when character polling occurs first.
    // This preserves key events for subsequent is_key_pressed()/is_key_down().
    if (!detail::key_queue.empty() && !detail::key_queue.front().is_char) {
      return 0;
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
    // Through the accessor, not the flag: it records that the edge was read,
    // which is what stops it being re-raised on the following frame.
    if (button == 0)
      return input_injector::is_mouse_button_pressed();
    if (button == 2)
      return input_injector::is_mouse_middle_button_pressed();
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
    if (button == 1)
      return input_injector::detail::mouse.right_down;
    if (button == 2)
      return input_injector::detail::mouse.middle_down;
    return false;
  }
  return backend_fn(button);
}

// Check mouse button released (wraps backend call)
template <typename BackendFn>
inline bool is_mouse_button_released(int button, BackendFn backend_fn) {
  if (detail::test_mode) {
    if (button == 0)
      return input_injector::detail::mouse.just_released;
    return false;
  }
  return backend_fn(button);
}

} // namespace test_input
} // namespace testing
} // namespace afterhours
