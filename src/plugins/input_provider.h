// Input Provider Interface for Afterhours
// Enables pluggable input backends for testing and cross-platform support.
//
// Usage:
//   // Use default raylib backend
//   auto& provider = afterhours::input_provider::get();
//   
//   // Set test mode for E2E testing
//   afterhours::input_provider::set_test_mode(true);
//   afterhours::input_provider::push_key(KEY_A);
//   afterhours::input_provider::set_mouse_position(100, 200);
//
// For custom backends, call set_provider() with your implementation.

#pragma once

#include <array>
#include <bitset>
#include <functional>
#include <optional>
#include <queue>

namespace afterhours {

// Forward declare types used by the interface
struct Vec2 {
  float x = 0, y = 0;
};

namespace input_provider {

//=============================================================================
// INPUT PROVIDER INTERFACE
//=============================================================================

/// Abstract interface for input backends
struct InputProvider {
  virtual ~InputProvider() = default;

  // Mouse input
  virtual Vec2 get_mouse_position() = 0;
  virtual bool is_mouse_button_pressed(int button) = 0;
  virtual bool is_mouse_button_down(int button) = 0;
  virtual bool is_mouse_button_released(int button) = 0;
  virtual bool is_mouse_button_up(int button) = 0;
  virtual float get_mouse_wheel_move() = 0;
  virtual Vec2 get_mouse_wheel_move_v() = 0;

  // Keyboard input
  virtual bool is_key_pressed(int key) = 0;
  virtual bool is_key_down(int key) = 0;
  virtual bool is_key_released(int key) = 0;
  virtual bool is_key_up(int key) = 0;
  virtual int get_char_pressed() = 0;

  // Frame management
  virtual void advance_frame() = 0;
};

//=============================================================================
// RAYLIB INPUT PROVIDER (Default)
//=============================================================================

#ifdef AFTER_HOURS_USE_RAYLIB
struct RaylibInputProvider : InputProvider {
  Vec2 get_mouse_position() override {
    auto pos = raylib::GetMousePosition();
    return {pos.x, pos.y};
  }

  bool is_mouse_button_pressed(int button) override {
    return raylib::IsMouseButtonPressed(button);
  }

  bool is_mouse_button_down(int button) override {
    return raylib::IsMouseButtonDown(button);
  }

  bool is_mouse_button_released(int button) override {
    return raylib::IsMouseButtonReleased(button);
  }

  bool is_mouse_button_up(int button) override {
    return raylib::IsMouseButtonUp(button);
  }

  float get_mouse_wheel_move() override {
    return raylib::GetMouseWheelMove();
  }

  Vec2 get_mouse_wheel_move_v() override {
    auto v = raylib::GetMouseWheelMoveV();
    return {v.x, v.y};
  }

  bool is_key_pressed(int key) override {
    return raylib::IsKeyPressed(key);
  }

  bool is_key_down(int key) override {
    return raylib::IsKeyDown(key);
  }

  bool is_key_released(int key) override {
    return raylib::IsKeyReleased(key);
  }

  bool is_key_up(int key) override {
    return raylib::IsKeyUp(key);
  }

  int get_char_pressed() override {
    return raylib::GetCharPressed();
  }

  void advance_frame() override {
    // No-op for raylib - frame advancement is handled by BeginDrawing/EndDrawing
  }
};
#endif

//=============================================================================
// TEST INPUT PROVIDER
//=============================================================================

/// Input provider for testing that allows injecting simulated input
struct TestInputProvider : InputProvider {
  static constexpr int MAX_KEYS = 512;
  static constexpr int MAX_BUTTONS = 8;

  // Mouse state
  Vec2 mouse_pos{0, 0};
  std::bitset<MAX_BUTTONS> buttons_down;
  std::bitset<MAX_BUTTONS> buttons_pressed;  // Just pressed this frame
  std::bitset<MAX_BUTTONS> buttons_released; // Just released this frame
  float wheel_move = 0.0f;
  Vec2 wheel_move_v{0, 0};

  // Keyboard state
  std::bitset<MAX_KEYS> keys_down;
  std::bitset<MAX_KEYS> keys_pressed;  // Just pressed this frame
  std::bitset<MAX_KEYS> keys_released; // Just released this frame
  
  // Character queue
  std::queue<int> char_queue;

  // Key press queue (for frame-accurate press detection)
  struct KeyPress {
    int key = 0;
    bool is_char = false;
    int char_value = 0;
  };
  std::queue<KeyPress> key_queue;

  // Implementation
  Vec2 get_mouse_position() override { return mouse_pos; }

  bool is_mouse_button_pressed(int button) override {
    if (button < 0 || button >= MAX_BUTTONS) return false;
    return buttons_pressed[button];
  }

  bool is_mouse_button_down(int button) override {
    if (button < 0 || button >= MAX_BUTTONS) return false;
    return buttons_down[button];
  }

  bool is_mouse_button_released(int button) override {
    if (button < 0 || button >= MAX_BUTTONS) return false;
    return buttons_released[button];
  }

  bool is_mouse_button_up(int button) override {
    if (button < 0 || button >= MAX_BUTTONS) return false;
    return !buttons_down[button];
  }

  float get_mouse_wheel_move() override { return wheel_move; }
  Vec2 get_mouse_wheel_move_v() override { return wheel_move_v; }

  bool is_key_pressed(int key) override {
    if (key < 0 || key >= MAX_KEYS) return false;
    return keys_pressed[key];
  }

  bool is_key_down(int key) override {
    if (key < 0 || key >= MAX_KEYS) return false;
    return keys_down[key];
  }

  bool is_key_released(int key) override {
    if (key < 0 || key >= MAX_KEYS) return false;
    return keys_released[key];
  }

  bool is_key_up(int key) override {
    if (key < 0 || key >= MAX_KEYS) return false;
    return !keys_down[key];
  }

  int get_char_pressed() override {
    if (char_queue.empty()) return 0;
    int c = char_queue.front();
    char_queue.pop();
    return c;
  }

  void advance_frame() override {
    // Clear per-frame state
    buttons_pressed.reset();
    buttons_released.reset();
    keys_pressed.reset();
    keys_released.reset();
    wheel_move = 0.0f;
    wheel_move_v = {0, 0};

    // Process queued key presses for this frame
    while (!key_queue.empty()) {
      auto& kp = key_queue.front();
      if (kp.is_char) {
        char_queue.push(kp.char_value);
      } else {
        if (kp.key >= 0 && kp.key < MAX_KEYS) {
          keys_pressed[kp.key] = true;
          keys_down[kp.key] = true;
        }
      }
      key_queue.pop();
    }
  }

  //===========================================================================
  // Test API - Methods for injecting test input
  //===========================================================================

  /// Set mouse position
  void set_mouse_position(float x, float y) {
    mouse_pos = {x, y};
  }

  /// Press mouse button (will be "pressed" this frame and "down" until released)
  void press_mouse_button(int button) {
    if (button < 0 || button >= MAX_BUTTONS) return;
    buttons_down[button] = true;
    buttons_pressed[button] = true;
  }

  /// Release mouse button
  void release_mouse_button(int button) {
    if (button < 0 || button >= MAX_BUTTONS) return;
    buttons_down[button] = false;
    buttons_released[button] = true;
  }

  /// Simulate a mouse click at position (press + release on next frame)
  void click(float x, float y, int button = 0) {
    set_mouse_position(x, y);
    press_mouse_button(button);
  }

  /// Set mouse wheel movement for this frame
  void scroll_wheel(float delta) {
    wheel_move = delta;
    wheel_move_v = {0, delta};
  }

  /// Set 2D mouse wheel movement (for trackpad horizontal scroll)
  void scroll_wheel_2d(float dx, float dy) {
    wheel_move = dy;
    wheel_move_v = {dx, dy};
  }

  /// Queue a key press (will be processed on next advance_frame)
  void push_key(int key) {
    KeyPress kp;
    kp.key = key;
    kp.is_char = false;
    key_queue.push(kp);
  }

  /// Queue a character input
  void push_char(int c) {
    KeyPress kp;
    kp.is_char = true;
    kp.char_value = c;
    key_queue.push(kp);
  }

  /// Hold a key down (until released)
  void hold_key(int key) {
    if (key >= 0 && key < MAX_KEYS) {
      keys_down[key] = true;
      keys_pressed[key] = true;
    }
  }

  /// Release a held key
  void release_key(int key) {
    if (key >= 0 && key < MAX_KEYS) {
      keys_down[key] = false;
      keys_released[key] = true;
    }
  }

  /// Reset all input state
  void reset() {
    mouse_pos = {0, 0};
    buttons_down.reset();
    buttons_pressed.reset();
    buttons_released.reset();
    wheel_move = 0;
    wheel_move_v = {0, 0};
    keys_down.reset();
    keys_pressed.reset();
    keys_released.reset();
    while (!char_queue.empty()) char_queue.pop();
    while (!key_queue.empty()) key_queue.pop();
  }
};

//=============================================================================
// GLOBAL PROVIDER MANAGEMENT
//=============================================================================

namespace detail {
  inline InputProvider* current_provider = nullptr;
  inline TestInputProvider test_provider;
  inline bool test_mode = false;

#ifdef AFTER_HOURS_USE_RAYLIB
  inline RaylibInputProvider raylib_provider;
#endif
}

/// Get the current input provider
inline InputProvider& get() {
  if (detail::test_mode) {
    return detail::test_provider;
  }
  if (detail::current_provider) {
    return *detail::current_provider;
  }
#ifdef AFTER_HOURS_USE_RAYLIB
  return detail::raylib_provider;
#else
  // Fallback to test provider in non-raylib builds
  return detail::test_provider;
#endif
}

/// Set a custom input provider
inline void set_provider(InputProvider* provider) {
  detail::current_provider = provider;
}

/// Enable/disable test mode (uses TestInputProvider)
inline void set_test_mode(bool enabled) {
  detail::test_mode = enabled;
  if (enabled) {
    detail::test_provider.reset();
  }
}

/// Check if test mode is active
inline bool is_test_mode() {
  return detail::test_mode;
}

/// Get the test input provider (for injecting test input)
inline TestInputProvider& get_test_provider() {
  return detail::test_provider;
}

//=============================================================================
// CONVENIENCE FUNCTIONS (delegate to current provider)
//=============================================================================

inline Vec2 get_mouse_position() { return get().get_mouse_position(); }
inline bool is_mouse_button_pressed(int b) { return get().is_mouse_button_pressed(b); }
inline bool is_mouse_button_down(int b) { return get().is_mouse_button_down(b); }
inline bool is_mouse_button_released(int b) { return get().is_mouse_button_released(b); }
inline bool is_mouse_button_up(int b) { return get().is_mouse_button_up(b); }
inline float get_mouse_wheel_move() { return get().get_mouse_wheel_move(); }
inline Vec2 get_mouse_wheel_move_v() { return get().get_mouse_wheel_move_v(); }
inline bool is_key_pressed(int k) { return get().is_key_pressed(k); }
inline bool is_key_down(int k) { return get().is_key_down(k); }
inline bool is_key_released(int k) { return get().is_key_released(k); }
inline bool is_key_up(int k) { return get().is_key_up(k); }
inline int get_char_pressed() { return get().get_char_pressed(); }
inline void advance_frame() { get().advance_frame(); }

// Test API convenience functions
inline void push_key(int key) { get_test_provider().push_key(key); }
inline void push_char(int c) { get_test_provider().push_char(c); }
inline void set_mouse_position(float x, float y) { get_test_provider().set_mouse_position(x, y); }
inline void press_mouse_button(int b = 0) { get_test_provider().press_mouse_button(b); }
inline void release_mouse_button(int b = 0) { get_test_provider().release_mouse_button(b); }
inline void click(float x, float y, int b = 0) { get_test_provider().click(x, y, b); }
inline void scroll_wheel(float delta) { get_test_provider().scroll_wheel(delta); }
inline void hold_key(int k) { get_test_provider().hold_key(k); }
inline void release_key(int k) { get_test_provider().release_key(k); }
inline void reset() { get_test_provider().reset(); }

} // namespace input_provider
} // namespace afterhours
