#pragma once

#include <array>
#include <optional>

#include "../border.h"
#include "../../color.h"

namespace afterhours {

namespace ui {

namespace imm {

using afterhours::ui::BorderPattern;

static constexpr BorderPattern None = BorderPattern::None;
static constexpr BorderPattern Solid = BorderPattern::Solid;
static constexpr BorderPattern Dotted = BorderPattern::Dotted;
static constexpr BorderPattern Dashed = BorderPattern::Dashed;

class BorderStyle {
private:
  BorderSideArray side_patterns;
  float line_thickness = 1.0f;

public:
  BorderStyle() {
    side_patterns.fill(BorderPattern::None);
  }

  BorderStyle &with_thickness(float thickness_px) {
    line_thickness = thickness_px;
    return *this;
  }

  BorderStyle &top(BorderPattern pattern) {
    side_patterns[static_cast<int>(BorderSide::Top)] = pattern;
    return *this;
  }
  BorderStyle &right(BorderPattern pattern) {
    side_patterns[static_cast<int>(BorderSide::Right)] = pattern;
    return *this;
  }
  BorderStyle &bottom(BorderPattern pattern) {
    side_patterns[static_cast<int>(BorderSide::Bottom)] = pattern;
    return *this;
  }
  BorderStyle &left(BorderPattern pattern) {
    side_patterns[static_cast<int>(BorderSide::Left)] = pattern;
    return *this;
  }

  const BorderSideArray &get_patterns() const { return side_patterns; }
  float thickness() const { return line_thickness; }

  bool any_enabled() const {
    for (auto p : side_patterns) {
      if (p != BorderPattern::None) return true;
    }
    return false;
  }
};

} // namespace imm

} // namespace ui

} // namespace afterhours