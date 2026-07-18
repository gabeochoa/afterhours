// E2E Testing Framework - Low-level Input Injector
// Synthetic key/mouse state for testing
#pragma once

#include "../../core/key_codes.h"
#include "concepts.h"

#include <array>

// TODO eventually match this exactly 1-1 with UI input system so we can share
// concepts
namespace afterhours {
namespace testing {
namespace input_injector {

namespace detail {
inline std::array<bool, 512> synthetic_keys{};
inline std::array<int, 512> synthetic_press_count{};
inline std::array<int, 512> synthetic_press_delay{};

struct MouseState {
  Position pos{}; // Uses Position from concepts.h (satisfies HasPosition)
  Position delta{};
  bool active = false;
  bool left_down = false;     // Matches UI convention
  bool just_pressed = false;  // Matches UI convention
  bool just_released = false; // Matches UI convention
  int press_frames = 0;       // Frame counter for multi-frame clicks
  bool auto_release = false;  // When true, release after press_frames expires
};
inline MouseState mouse;

// Synthetic scroll wheel state (consumed once per frame)
struct WheelState {
  float x = 0.0f;
  float y = 0.0f;
};
inline WheelState wheel;

struct PendingClick {
  bool pending = false;
  float x = 0, y = 0;
};
inline PendingClick pending_click;

struct KeyHold {
  bool active = false;
  int keycode = 0;
  float remaining = 0.0f;
};
inline KeyHold key_hold;
} // namespace detail

/// Set a key as synthetically held down
inline void set_key_down(int key) {
  if (key >= 0 && key < 512) {
    detail::synthetic_keys[key] = true;
    detail::synthetic_press_count[key]++;
    detail::synthetic_press_delay[key] = 1;
  }
}

/// Set a key as synthetically held without generating a press event.
/// Use for modifier keys that need to be "down" for chord matching
/// but should NOT appear as "pressed" actions in the InputCollector.
inline void set_key_held(int key) {
  if (key >= 0 && key < 512) {
    detail::synthetic_keys[key] = true;
  }
}

/// Release a synthetically held key
inline void set_key_up(int key) {
  if (key >= 0 && key < 512) {
    detail::synthetic_keys[key] = false;
  }
}

/// Check if key is synthetically held
inline bool is_key_down(int key) {
  return key >= 0 && key < 512 && detail::synthetic_keys[key];
}

/// Check if a synthetic key press is available this frame.
/// The press remains available for the entire frame so that multiple
/// action mappings sharing the same key all see it.  The count is
/// decremented once per frame in reset_frame().
inline bool consume_press(int key) {
  if (key < 0 || key >= 512)
    return false;
  return detail::synthetic_press_count[key] > 0 &&
         detail::synthetic_press_delay[key] == 0;
}

/// Hold a key for specified duration (seconds)
inline void hold_key_for_duration(int key, float duration) {
  set_key_down(key);
  detail::key_hold = {true, key, duration};
}

/// Update timed key holds (call each frame with delta time)
inline void update_key_hold(float dt) {
  if (detail::key_hold.active) {
    detail::key_hold.remaining -= dt;
    if (detail::key_hold.remaining <= 0) {
      set_key_up(detail::key_hold.keycode);
      detail::key_hold.active = false;
    }
  }
}

/// Set mouse position
inline void set_mouse_position(float x, float y) {
  if (detail::mouse.active) {
    detail::mouse.delta.x = x - detail::mouse.pos.x;
    detail::mouse.delta.y = y - detail::mouse.pos.y;
  } else {
    // First synthetic position has no previous point to diff against.
    detail::mouse.delta = {};
  }
  detail::mouse.pos.x = x;
  detail::mouse.pos.y = y;
  detail::mouse.active = true;
}

/// Set mouse position from any HasPosition type
template <HasPosition T> inline void set_mouse_position(const T &pos) {
  set_mouse_position(static_cast<float>(pos.x), static_cast<float>(pos.y));
}

/// Get mouse position
inline void get_mouse_position(float &x, float &y) {
  x = detail::mouse.pos.x;
  y = detail::mouse.pos.y;
}

/// Get mouse position as Position
inline Position get_mouse_position() { return detail::mouse.pos; }

/// Consume synthetic mouse delta (returns and clears it)
inline Position consume_mouse_delta() {
  Position d = detail::mouse.delta;
  detail::mouse.delta = {};
  return d;
}

/// Schedule a click at center of rectangle
inline void schedule_click_at(float x, float y, float w, float h) {
  detail::pending_click = {true, x + w / 2, y + h / 2};
}

/// Execute scheduled click
inline void inject_scheduled_click() {
  if (detail::pending_click.pending) {
    set_mouse_position(detail::pending_click.x, detail::pending_click.y);
    detail::mouse.left_down = true;
    detail::mouse.just_pressed = true;
  }
}

/// Release scheduled click
inline void release_scheduled_click() {
  if (detail::pending_click.pending && detail::mouse.left_down) {
    detail::mouse.left_down = false;
    detail::mouse.just_pressed = false;
    detail::mouse.just_released = true;
    detail::pending_click.pending = false;
  }
}

/// Check mouse button state
inline bool is_mouse_button_pressed() {
  return detail::mouse.active && detail::mouse.just_pressed;
}
inline bool is_mouse_button_down() {
  return detail::mouse.active && detail::mouse.left_down;
}
inline bool is_mouse_button_released() {
  return detail::mouse.active && detail::mouse.just_released;
}

/// Set scroll wheel delta (consumed on next frame)
inline void set_mouse_wheel(float dx, float dy) {
  detail::wheel.x = dx;
  detail::wheel.y = dy;
  // Wheel injection should be observable even if no prior mouse move occurred.
  detail::mouse.active = true;
}

/// Consume the synthetic wheel delta (returns and clears it)
inline Position consume_wheel() {
  Position v{detail::wheel.x, detail::wheel.y};
  detail::wheel = {};
  return v;
}

/// Reset per-frame state (call at start of frame)
inline void reset_frame() {
  detail::mouse.just_pressed = false;
  detail::mouse.just_released = false;
  detail::mouse.delta = {};
  // Tick key press delays and consume presses from the previous frame.
  // Delays are decremented here so that all callers within a single
  // frame see the same delay value.  Counts are decremented here so
  // that a press is available for the entire frame it fires on.
  for (int i = 0; i < 512; i++) {
    if (detail::synthetic_press_delay[i] > 0) {
      detail::synthetic_press_delay[i]--;
    } else if (detail::synthetic_press_count[i] > 0) {
      detail::synthetic_press_count[i]--;
    }
  }
}

/// Clear all synthetic input state
inline void reset_all() {
  detail::synthetic_keys.fill(false);
  detail::synthetic_press_count.fill(0);
  detail::synthetic_press_delay.fill(0);
  detail::mouse = {};
  detail::pending_click = {};
  detail::key_hold = {};
  detail::wheel = {};
}

} // namespace input_injector

// Shared state for shift_tab command (needs to be accessible from both
// ui_commands.h and command_handlers.h)
namespace shift_tab_detail {
inline int release_countdown = 0;
inline void reset() { release_countdown = 0; }
} // namespace shift_tab_detail

} // namespace testing
} // namespace afterhours
