// E2E Testing Framework - Platform-aware Test Input
//
// Provides complete test-aware input wrappers that:
//   1. Check E2E injection state (synthetic keys/mouse) first
//   2. Fall through to the active PlatformAPI for real input
//
// This replaces the need for per-project backend-specific wrappers.
// Just include this header and use afterhours::testing::platform_input::.
//
// Requires: AFTER_HOURS_USE_RAYLIB or AFTER_HOURS_USE_METAL to be defined,
//           and the corresponding graphics.h to be included before this header
//           (or it will be included automatically).

#pragma once

#include "input_injector.h"
#include "test_input.h"
#include "visible_text.h"

#include "../../developer.h"
#include "../../graphics.h"

namespace afterhours {
namespace testing {
namespace platform_input {

// ============================================================
// Re-export core test_input state management
// ============================================================

namespace detail {
using afterhours::testing::test_input::detail::key_queue;
using afterhours::testing::test_input::detail::test_mode;
} // namespace detail

inline void set_test_mode(bool enabled) {
  afterhours::testing::test_input::detail::test_mode = enabled;
}
inline bool is_test_mode() {
  return afterhours::testing::test_input::detail::test_mode;
}

// ============================================================
// Visible text registration (for E2E assertions)
// ============================================================

inline void register_visible_text(const std::string &text) {
  afterhours::testing::VisibleTextRegistry::instance().register_text(text);
}
inline void clear_visible_text_registry() {
  afterhours::testing::VisibleTextRegistry::instance().clear();
}

// ============================================================
// Re-export queue/simulation helpers
// ============================================================

using afterhours::testing::test_input::clear_queue;
using afterhours::testing::test_input::push_char;
using afterhours::testing::test_input::push_key;
using afterhours::testing::test_input::reset_all;
using afterhours::testing::test_input::reset_frame;
using afterhours::testing::test_input::simulate_arrow_down;
using afterhours::testing::test_input::simulate_arrow_left;
using afterhours::testing::test_input::simulate_arrow_right;
using afterhours::testing::test_input::simulate_arrow_up;
using afterhours::testing::test_input::simulate_backspace;
using afterhours::testing::test_input::simulate_click;
using afterhours::testing::test_input::simulate_enter;
using afterhours::testing::test_input::simulate_escape;
using afterhours::testing::test_input::simulate_mouse_press;
using afterhours::testing::test_input::simulate_mouse_release;
using afterhours::testing::test_input::simulate_tab;

// ============================================================
// Keyboard input (test-aware, delegates to PlatformAPI)
// ============================================================

inline bool is_key_pressed(int key) {
  return afterhours::testing::test_input::is_key_pressed(
      key, [](int k) { return afterhours::graphics::is_key_pressed(k); });
}

inline bool is_key_down(int key) {
  return afterhours::testing::test_input::is_key_down(
      key, [](int k) { return afterhours::graphics::is_key_down(k); });
}

inline int get_char_pressed() {
  return afterhours::testing::test_input::get_char_pressed(
      []() { return afterhours::graphics::get_char_pressed(); });
}

// ============================================================
// Mouse input (test-aware, delegates to PlatformAPI)
// ============================================================

inline bool is_mouse_button_pressed(int button) {
  if (button == 0 && afterhours::testing::test_input::detail::test_mode) {
    return afterhours::testing::input_injector::is_mouse_button_pressed();
  }
  return afterhours::graphics::is_mouse_button_pressed(button);
}

inline bool is_mouse_button_down(int button) {
  if (button == 0 && afterhours::testing::test_input::detail::test_mode) {
    return afterhours::testing::input_injector::is_mouse_button_down();
  }
  return afterhours::graphics::is_mouse_button_down(button);
}

inline bool is_mouse_button_released(int button) {
  if (button == 0 && afterhours::testing::test_input::detail::test_mode) {
    return afterhours::testing::input_injector::is_mouse_button_released();
  }
  return afterhours::graphics::is_mouse_button_released(button);
}

inline bool is_mouse_button_up(int button) {
  return !is_mouse_button_down(button);
}

inline Vector2Type get_mouse_position() {
  return afterhours::testing::test_input::get_mouse_position<Vector2Type>([]() {
    auto p = afterhours::graphics::get_mouse_position();
    return Vector2Type{p.x, p.y};
  });
}

inline float get_mouse_wheel_move() {
  if (afterhours::testing::test_input::detail::test_mode)
    return 0.0f;
  return afterhours::graphics::get_mouse_wheel_move();
}

// ============================================================
// Mouse position helpers (for E2E scripts)
// ============================================================

inline void set_mouse_position(float x, float y) {
  afterhours::testing::test_input::set_mouse_position(x, y);
}

} // namespace platform_input
} // namespace testing
} // namespace afterhours
