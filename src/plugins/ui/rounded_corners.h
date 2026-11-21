#pragma once

#include <bitset>

namespace afterhours {

namespace ui {

namespace imm {

// Corner positions in the bitset. This layout is the one both backends
// read (raylib's DrawRectangleCustom order, the sokol fan) and every UI
// bitset reaches them unchanged, so the names must match it: bit 3 is
// top-left, not bit 0.
enum CornerPosition {
  TOP_LEFT = 3,
  TOP_RIGHT = 2,
  BOTTOM_LEFT = 1,
  BOTTOM_RIGHT = 0
};

// Corner state
enum CornerState {
  ROUND = true,
  SHARP = false // shorter than NOT_ROUNDED
};

class RoundedCorners {
private:
  std::bitset<4> corners;
  std::bitset<4> chamfered;

public:
  RoundedCorners() : corners(std::bitset<4>().set()) {} // default all rounded
  RoundedCorners(const std::bitset<4> &base) : corners(base) {}

  RoundedCorners &chamfer(CornerPosition corner) {
    chamfered.set(corner);
    return *this;
  }
  RoundedCorners &chamfer_left() {
    chamfered.set(TOP_LEFT).set(BOTTOM_LEFT);
    return *this;
  }
  RoundedCorners &all_chamfer() {
    chamfered.set();
    return *this;
  }
  const std::bitset<4> &get_chamfered() const { return chamfered; }
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

  RoundedCorners &round(CornerPosition corner) {
    corners.set(corner, ROUND);
    return *this;
  }

  RoundedCorners &sharp(CornerPosition corner) {
    corners.set(corner, SHARP);
    return *this;
  }

  RoundedCorners &all_round() {
    corners.set();
    return *this;
  }

  RoundedCorners &all_sharp() {
    corners.reset();
    return *this;
  }

  RoundedCorners &left_round() {
    corners.set(TOP_RIGHT, SHARP);
    corners.set(BOTTOM_RIGHT, SHARP);
    corners.set(TOP_LEFT, ROUND);
    corners.set(BOTTOM_LEFT, ROUND);
    return *this;
  }

  RoundedCorners &right_round() {
    corners.set(TOP_LEFT, SHARP);
    corners.set(BOTTOM_LEFT, SHARP);
    corners.set(TOP_RIGHT, ROUND);
    corners.set(BOTTOM_RIGHT, ROUND);
    return *this;
  }

  RoundedCorners &top_round() {
    corners.set(TOP_LEFT, ROUND);
    corners.set(TOP_RIGHT, ROUND);
    corners.set(BOTTOM_RIGHT, ROUND);
    corners.set(BOTTOM_LEFT, SHARP);
    return *this;
  }

  RoundedCorners &bottom_round() {
    corners.set(TOP_LEFT, SHARP);
    corners.set(TOP_RIGHT, SHARP);
    corners.set(BOTTOM_LEFT, ROUND);
    corners.set(BOTTOM_RIGHT, ROUND);
    return *this;
  }

  RoundedCorners &left_sharp() {
    corners.set(TOP_LEFT, SHARP);
    corners.set(BOTTOM_LEFT, SHARP);
    corners.set(TOP_RIGHT, ROUND);
    corners.set(BOTTOM_RIGHT, ROUND);
    return *this;
  }

  RoundedCorners &right_sharp() {
    corners.set(TOP_RIGHT, SHARP);
    corners.set(BOTTOM_RIGHT, SHARP);
    corners.set(TOP_LEFT, ROUND);
    corners.set(BOTTOM_LEFT, ROUND);
    return *this;
  }

  RoundedCorners &top_sharp() {
    corners.set(TOP_LEFT, SHARP);
    corners.set(TOP_RIGHT, SHARP);
    corners.set(BOTTOM_RIGHT, ROUND);
    corners.set(BOTTOM_LEFT, ROUND);
    return *this;
  }

  RoundedCorners &bottom_sharp() {
    corners.set(TOP_LEFT, ROUND);
    corners.set(TOP_RIGHT, ROUND);
    corners.set(BOTTOM_LEFT, SHARP);
    corners.set(BOTTOM_RIGHT, SHARP);
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
