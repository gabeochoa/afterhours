#pragma once

#include "../../../../vendor/catch2/catch.hpp"
#include "../../../../src/plugins/autolayout.h"

namespace afterhours {
namespace ui {
namespace component_tests {

TEST_CASE("ButtonComponentSizeBasic", "[AutoLayout][ComponentSize][Button]") {
  // Test basic Button component autolayout with standard button size
  AutoLayout al; // Empty mapping
  
  // Create a UIComponent representing a Button
  UIComponent button_cmp(1);
  
  // Create ComponentSize with pixels() setting for standard Button (140x40)
  ComponentSize button_size = ComponentSize{pixels(140.f), pixels(40.f)};
  
  // Apply the ComponentSize to the UIComponent's desired sizes
  button_cmp.desired[Axis::X] = button_size.x_axis;
  button_cmp.desired[Axis::Y] = button_size.y_axis;
  
  // Verify the size was set correctly with pixels dimension
  REQUIRE(button_cmp.desired[Axis::X].dim == Dim::Pixels);
  REQUIRE(button_cmp.desired[Axis::Y].dim == Dim::Pixels);
  REQUIRE(button_cmp.desired[Axis::X].value == 140.f);
  REQUIRE(button_cmp.desired[Axis::Y].value == 40.f);
  
  // Run autolayout calculations
  al.calculate_standalone(button_cmp);
  
  // Verify computed values match the pixels() settings
  REQUIRE(button_cmp.computed[Axis::X] == 140.f);
  REQUIRE(button_cmp.computed[Axis::Y] == 40.f);
}

TEST_CASE("ButtonComponentSizeVariants", "[AutoLayout][ComponentSize][Button]") {
  // Test Button component with various typical button sizes
  AutoLayout al; // Empty mapping
  
  // Test small button (like icon button)
  UIComponent small_button(1);
  ComponentSize small_size = ComponentSize{pixels(32.f), pixels(32.f)};
  small_button.desired[Axis::X] = small_size.x_axis;
  small_button.desired[Axis::Y] = small_size.y_axis;
  
  al.calculate_standalone(small_button);
  
  REQUIRE(small_button.computed[Axis::X] == 32.f);
  REQUIRE(small_button.computed[Axis::Y] == 32.f);
  
  // Test medium button (common UI button)
  UIComponent medium_button(2);
  ComponentSize medium_size = ComponentSize{pixels(100.f), pixels(30.f)};
  medium_button.desired[Axis::X] = medium_size.x_axis;
  medium_button.desired[Axis::Y] = medium_size.y_axis;
  
  al.calculate_standalone(medium_button);
  
  REQUIRE(medium_button.computed[Axis::X] == 100.f);
  REQUIRE(medium_button.computed[Axis::Y] == 30.f);
  
  // Test large button (primary action button)
  UIComponent large_button(3);
  ComponentSize large_size = ComponentSize{pixels(200.f), pixels(50.f)};
  large_button.desired[Axis::X] = large_size.x_axis;
  large_button.desired[Axis::Y] = large_size.y_axis;
  
  al.calculate_standalone(large_button);
  
  REQUIRE(large_button.computed[Axis::X] == 200.f);
  REQUIRE(large_button.computed[Axis::Y] == 50.f);
  
  // Test wide button (full-width style)
  UIComponent wide_button(4);
  ComponentSize wide_size = ComponentSize{pixels(300.f), pixels(40.f)};
  wide_button.desired[Axis::X] = wide_size.x_axis;
  wide_button.desired[Axis::Y] = wide_size.y_axis;
  
  al.calculate_standalone(wide_button);
  
  REQUIRE(wide_button.computed[Axis::X] == 300.f);
  REQUIRE(wide_button.computed[Axis::Y] == 40.f);
}

TEST_CASE("ButtonComponentSizePrecision", "[AutoLayout][ComponentSize][Button]") {
  // Test Button component with fractional pixel values for precision testing
  AutoLayout al; // Empty mapping
  
  UIComponent precise_button(1);
  ComponentSize precise_size = ComponentSize{pixels(125.5f), pixels(42.25f)};
  precise_button.desired[Axis::X] = precise_size.x_axis;
  precise_button.desired[Axis::Y] = precise_size.y_axis;
  
  al.calculate_standalone(precise_button);
  
  REQUIRE(precise_button.computed[Axis::X] == Approx(125.5f));
  REQUIRE(precise_button.computed[Axis::Y] == Approx(42.25f));
  
  // Test very small fractional button
  UIComponent tiny_button(2);
  ComponentSize tiny_size = ComponentSize{pixels(24.75f), pixels(18.5f)};
  tiny_button.desired[Axis::X] = tiny_size.x_axis;
  tiny_button.desired[Axis::Y] = tiny_size.y_axis;
  
  al.calculate_standalone(tiny_button);
  
  REQUIRE(tiny_button.computed[Axis::X] == Approx(24.75f));
  REQUIRE(tiny_button.computed[Axis::Y] == Approx(18.5f));
}

} // namespace component_tests
} // namespace ui
} // namespace afterhours