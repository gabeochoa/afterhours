#pragma once

#include "../../../../vendor/catch2/catch.hpp"
#include "../../../../src/plugins/autolayout.h"

namespace afterhours {
namespace ui {
namespace component_tests {
namespace percent_tests {

TEST_CASE("ButtonComponentPercentBasic", "[AutoLayout][ComponentSize][Button][Percent]") {
  AutoLayout al;
  UIComponent button_cmp(1);
  ComponentSize button_size = ComponentSize{percent(0.5f), percent(0.1f)};
  button_cmp.desired[Axis::X] = button_size.x_axis;
  button_cmp.desired[Axis::Y] = button_size.y_axis;
  
  REQUIRE(button_cmp.desired[Axis::X].dim == Dim::Percent);
  REQUIRE(button_cmp.desired[Axis::Y].dim == Dim::Percent);
  REQUIRE(button_cmp.desired[Axis::X].value == 0.5f);
  REQUIRE(button_cmp.desired[Axis::Y].value == 0.1f);
  
  al.calculate_standalone(button_cmp);
  // Percent calculations without parent result in -1
  REQUIRE(button_cmp.computed[Axis::X] == -1.f);
  REQUIRE(button_cmp.computed[Axis::Y] == -1.f);
}

TEST_CASE("ButtonComponentPercentVariants", "[AutoLayout][ComponentSize][Button][Percent]") {
  AutoLayout al;
  
  UIComponent small_button(1);
  ComponentSize small_size = ComponentSize{percent(0.25f), percent(0.08f)};
  small_button.desired[Axis::X] = small_size.x_axis;
  small_button.desired[Axis::Y] = small_size.y_axis;
  al.calculate_standalone(small_button);
  REQUIRE(small_button.desired[Axis::X].value == 0.25f);
  REQUIRE(small_button.desired[Axis::Y].value == 0.08f);
  REQUIRE(small_button.computed[Axis::X] == -1.f);
  REQUIRE(small_button.computed[Axis::Y] == -1.f);
  
  UIComponent large_button(2);
  ComponentSize large_size = ComponentSize{percent(0.6f), percent(0.15f)};
  large_button.desired[Axis::X] = large_size.x_axis;
  large_button.desired[Axis::Y] = large_size.y_axis;
  al.calculate_standalone(large_button);
  REQUIRE(large_button.desired[Axis::X].value == 0.6f);
  REQUIRE(large_button.desired[Axis::Y].value == 0.15f);
  REQUIRE(large_button.computed[Axis::X] == -1.f);
  REQUIRE(large_button.computed[Axis::Y] == -1.f);
}

TEST_CASE("ButtonComponentPercentPrecision", "[AutoLayout][ComponentSize][Button][Percent]") {
  AutoLayout al;
  
  UIComponent precise_button(1);
  ComponentSize precise_size = ComponentSize{percent(0.375f), percent(0.125f)};
  precise_button.desired[Axis::X] = precise_size.x_axis;
  precise_button.desired[Axis::Y] = precise_size.y_axis;
  al.calculate_standalone(precise_button);
  REQUIRE(precise_button.desired[Axis::X].value == Approx(0.375f));
  REQUIRE(precise_button.desired[Axis::Y].value == Approx(0.125f));
  REQUIRE(precise_button.computed[Axis::X] == -1.f);
  REQUIRE(precise_button.computed[Axis::Y] == -1.f);
}

} // namespace percent_tests
} // namespace component_tests
} // namespace ui
} // namespace afterhours