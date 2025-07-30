#pragma once

#include <bitset>

namespace afterhours {

namespace ui {

namespace imm {

// Corner positions in the bitset
enum CornerPosition {
  TOP_LEFT = 0,
  TOP_RIGHT = 1,
  BOTTOM_LEFT = 2,
  BOTTOM_RIGHT = 3
};

// Corner state
enum CornerState {
  ROUND = true,
  SHARP = false // shorter than NOT_ROUNDED
};

class RoundedCorners {
private:
  std::bitset<4> corners;

public:
  RoundedCorners() : corners(std::bitset<4>().set()) {} // default all rounded
  RoundedCorners(const std::bitset<4> &base) : corners(base) {}

  // Fluent API for modifying corners
  RoundedCorners &top_left(CornerState state) {
    corners.set(TOP_LEFT, state);
    return *this;
  }

  RoundedCorners &top_right(CornerState state) {
    corners.set(TOP_RIGHT, state);
    return *this;
  }

  RoundedCorners &bottom_left(CornerState state) {
    corners.set(BOTTOM_LEFT, state);
    return *this;
  }

  RoundedCorners &bottom_right(CornerState state) {
    corners.set(BOTTOM_RIGHT, state);
    return *this;
  }

  // Alternative shorter API
  RoundedCorners &round(CornerPosition corner) {
    corners.set(corner, ROUND);
    return *this;
  }

  RoundedCorners &sharp(CornerPosition corner) {
    corners.set(corner, SHARP);
    return *this;
  }

  // Conversion to bitset
  operator std::bitset<4>() const { return corners; }

  // Get the underlying bitset
  const std::bitset<4> &get() const { return corners; }
};

} // namespace imm

} // namespace ui

} // namespace afterhours