#pragma once

#include <array>

namespace afterhours {

namespace ui {

// Order matches: TOP, RIGHT, BOTTOM, LEFT
enum struct BorderPattern {
  None = 0,
  Solid = 1,
  Dotted = 2,
  Dashed = 3, // reserved for future
};

enum struct BorderSide {
  Top = 0,
  Right = 1,
  Bottom = 2,
  Left = 3,
};

using BorderSideArray = std::array<BorderPattern, 4>;

} // namespace ui

} // namespace afterhours