// E2E Testing Framework - Concepts
// Type constraints for input-related types
#pragma once

#include <concepts>

namespace afterhours {
namespace testing {

/// Concept for any type with x and y coordinate members
template <typename T>
concept HasPosition = requires(T t) {
  { t.x } -> std::convertible_to<float>;
  { t.y } -> std::convertible_to<float>;
};

/// Concept for mouse state with position and button state
template <typename T>
concept MouseStateLike = HasPosition<T> && requires(T t) {
  { t.left_down } -> std::convertible_to<bool>;
};

/// Concept for full mouse pointer state (UI-style)
template <typename T>
concept MousePointerStateLike = requires(T t) {
  { t.pos } -> HasPosition;
  { t.left_down } -> std::convertible_to<bool>;
  { t.just_pressed } -> std::convertible_to<bool>;
  { t.just_released } -> std::convertible_to<bool>;
};

/// Simple position struct that satisfies HasPosition
/// Use this when you don't want to depend on raylib::Vector2
struct Position {
  float x = 0;
  float y = 0;

  Position() = default;
  Position(float x_, float y_) : x(x_), y(y_) {}

  // Implicit conversion from any HasPosition type
  template <HasPosition T>
  Position(const T &other)
      : x(static_cast<float>(other.x)), y(static_cast<float>(other.y)) {}
};

static_assert(HasPosition<Position>, "Position must satisfy HasPosition");

} // namespace testing
} // namespace afterhours
