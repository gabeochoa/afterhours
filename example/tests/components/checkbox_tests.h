#pragma once

#include "../../../vendor/catch2/catch.hpp"
#include "../../../src/plugins/autolayout.h"

namespace afterhours {
namespace ui {
namespace component_tests {

TEST_CASE("CheckboxComponentSizeBasic", "[AutoLayout][ComponentSize][Checkbox]") {
  // Test basic Checkbox component autolayout
  AutoLayout al; // Empty mapping
  
  // Create a UIComponent representing a Checkbox
  UIComponent checkbox_cmp(1);
  
  // Standard checkbox with label size (checkbox + text)
  ComponentSize checkbox_size = ComponentSize{pixels(120.f), pixels(24.f)};
  
  // Apply the ComponentSize to the UIComponent's desired sizes
  checkbox_cmp.desired[Axis::X] = checkbox_size.x_axis;
  checkbox_cmp.desired[Axis::Y] = checkbox_size.y_axis;
  
  // Verify the size was set correctly with pixels dimension
  REQUIRE(checkbox_cmp.desired[Axis::X].dim == Dim::Pixels);
  REQUIRE(checkbox_cmp.desired[Axis::Y].dim == Dim::Pixels);
  REQUIRE(checkbox_cmp.desired[Axis::X].value == 120.f);
  REQUIRE(checkbox_cmp.desired[Axis::Y].value == 24.f);
  
  // Run autolayout calculations
  al.calculate_standalone(checkbox_cmp);
  
  // Verify computed values match the pixels() settings
  REQUIRE(checkbox_cmp.computed[Axis::X] == 120.f);
  REQUIRE(checkbox_cmp.computed[Axis::Y] == 24.f);
}

TEST_CASE("CheckboxComponentSizeVariants", "[AutoLayout][ComponentSize][Checkbox]") {
  // Test Checkbox component with various typical sizes
  AutoLayout al; // Empty mapping
  
  // Test small checkbox (compact form)
  UIComponent small_checkbox(1);
  ComponentSize small_size = ComponentSize{pixels(80.f), pixels(18.f)};
  small_checkbox.desired[Axis::X] = small_size.x_axis;
  small_checkbox.desired[Axis::Y] = small_size.y_axis;
  
  al.calculate_standalone(small_checkbox);
  
  REQUIRE(small_checkbox.computed[Axis::X] == 80.f);
  REQUIRE(small_checkbox.computed[Axis::Y] == 18.f);
  
  // Test standard checkbox (most common)
  UIComponent standard_checkbox(2);
  ComponentSize standard_size = ComponentSize{pixels(100.f), pixels(22.f)};
  standard_checkbox.desired[Axis::X] = standard_size.x_axis;
  standard_checkbox.desired[Axis::Y] = standard_size.y_axis;
  
  al.calculate_standalone(standard_checkbox);
  
  REQUIRE(standard_checkbox.computed[Axis::X] == 100.f);
  REQUIRE(standard_checkbox.computed[Axis::Y] == 22.f);
  
  // Test large checkbox (accessibility)
  UIComponent large_checkbox(3);
  ComponentSize large_size = ComponentSize{pixels(160.f), pixels(28.f)};
  large_checkbox.desired[Axis::X] = large_size.x_axis;
  large_checkbox.desired[Axis::Y] = large_size.y_axis;
  
  al.calculate_standalone(large_checkbox);
  
  REQUIRE(large_checkbox.computed[Axis::X] == 160.f);
  REQUIRE(large_checkbox.computed[Axis::Y] == 28.f);
  
  // Test long label checkbox
  UIComponent long_checkbox(4);
  ComponentSize long_size = ComponentSize{pixels(250.f), pixels(24.f)};
  long_checkbox.desired[Axis::X] = long_size.x_axis;
  long_checkbox.desired[Axis::Y] = long_size.y_axis;
  
  al.calculate_standalone(long_checkbox);
  
  REQUIRE(long_checkbox.computed[Axis::X] == 250.f);
  REQUIRE(long_checkbox.computed[Axis::Y] == 24.f);
}

TEST_CASE("CheckboxComponentSizeBoxOnly", "[AutoLayout][ComponentSize][Checkbox]") {
  // Test Checkbox component for checkbox box only (no label)
  AutoLayout al; // Empty mapping
  
  // Test tiny checkbox box
  UIComponent tiny_box(1);
  ComponentSize tiny_size = ComponentSize{pixels(12.f), pixels(12.f)};
  tiny_box.desired[Axis::X] = tiny_size.x_axis;
  tiny_box.desired[Axis::Y] = tiny_size.y_axis;
  
  al.calculate_standalone(tiny_box);
  
  REQUIRE(tiny_box.computed[Axis::X] == 12.f);
  REQUIRE(tiny_box.computed[Axis::Y] == 12.f);
  
  // Test small checkbox box
  UIComponent small_box(2);
  ComponentSize small_size = ComponentSize{pixels(16.f), pixels(16.f)};
  small_box.desired[Axis::X] = small_size.x_axis;
  small_box.desired[Axis::Y] = small_size.y_axis;
  
  al.calculate_standalone(small_box);
  
  REQUIRE(small_box.computed[Axis::X] == 16.f);
  REQUIRE(small_box.computed[Axis::Y] == 16.f);
  
  // Test standard checkbox box
  UIComponent standard_box(3);
  ComponentSize standard_size = ComponentSize{pixels(20.f), pixels(20.f)};
  standard_box.desired[Axis::X] = standard_size.x_axis;
  standard_box.desired[Axis::Y] = standard_size.y_axis;
  
  al.calculate_standalone(standard_box);
  
  REQUIRE(standard_box.computed[Axis::X] == 20.f);
  REQUIRE(standard_box.computed[Axis::Y] == 20.f);
  
  // Test large checkbox box
  UIComponent large_box(4);
  ComponentSize large_size = ComponentSize{pixels(24.f), pixels(24.f)};
  large_box.desired[Axis::X] = large_size.x_axis;
  large_box.desired[Axis::Y] = large_size.y_axis;
  
  al.calculate_standalone(large_box);
  
  REQUIRE(large_box.computed[Axis::X] == 24.f);
  REQUIRE(large_box.computed[Axis::Y] == 24.f);
}

TEST_CASE("CheckboxComponentSizeFormLayouts", "[AutoLayout][ComponentSize][Checkbox]") {
  // Test Checkbox component sizes for different form layouts
  AutoLayout al; // Empty mapping
  
  // Test inline form checkbox
  UIComponent inline_checkbox(1);
  ComponentSize inline_size = ComponentSize{pixels(90.f), pixels(20.f)};
  inline_checkbox.desired[Axis::X] = inline_size.x_axis;
  inline_checkbox.desired[Axis::Y] = inline_size.y_axis;
  
  al.calculate_standalone(inline_checkbox);
  
  REQUIRE(inline_checkbox.computed[Axis::X] == 90.f);
  REQUIRE(inline_checkbox.computed[Axis::Y] == 20.f);
  
  // Test vertical form checkbox
  UIComponent vertical_checkbox(2);
  ComponentSize vertical_size = ComponentSize{pixels(180.f), pixels(26.f)};
  vertical_checkbox.desired[Axis::X] = vertical_size.x_axis;
  vertical_checkbox.desired[Axis::Y] = vertical_size.y_axis;
  
  al.calculate_standalone(vertical_checkbox);
  
  REQUIRE(vertical_checkbox.computed[Axis::X] == 180.f);
  REQUIRE(vertical_checkbox.computed[Axis::Y] == 26.f);
  
  // Test grid form checkbox
  UIComponent grid_checkbox(3);
  ComponentSize grid_size = ComponentSize{pixels(200.f), pixels(24.f)};
  grid_checkbox.desired[Axis::X] = grid_size.x_axis;
  grid_checkbox.desired[Axis::Y] = grid_size.y_axis;
  
  al.calculate_standalone(grid_checkbox);
  
  REQUIRE(grid_checkbox.computed[Axis::X] == 200.f);
  REQUIRE(grid_checkbox.computed[Axis::Y] == 24.f);
  
  // Test settings panel checkbox
  UIComponent settings_checkbox(4);
  ComponentSize settings_size = ComponentSize{pixels(300.f), pixels(32.f)};
  settings_checkbox.desired[Axis::X] = settings_size.x_axis;
  settings_checkbox.desired[Axis::Y] = settings_size.y_axis;
  
  al.calculate_standalone(settings_checkbox);
  
  REQUIRE(settings_checkbox.computed[Axis::X] == 300.f);
  REQUIRE(settings_checkbox.computed[Axis::Y] == 32.f);
}

TEST_CASE("CheckboxComponentSizePrecision", "[AutoLayout][ComponentSize][Checkbox]") {
  // Test Checkbox component with fractional pixel values
  AutoLayout al; // Empty mapping
  
  UIComponent precise_checkbox(1);
  ComponentSize precise_size = ComponentSize{pixels(118.25f), pixels(22.75f)};
  precise_checkbox.desired[Axis::X] = precise_size.x_axis;
  precise_checkbox.desired[Axis::Y] = precise_size.y_axis;
  
  al.calculate_standalone(precise_checkbox);
  
  REQUIRE(precise_checkbox.computed[Axis::X] == Approx(118.25f));
  REQUIRE(precise_checkbox.computed[Axis::Y] == Approx(22.75f));
  
  // Test precise checkbox box only
  UIComponent precise_box(2);
  ComponentSize precise_box_size = ComponentSize{pixels(18.5f), pixels(18.5f)};
  precise_box.desired[Axis::X] = precise_box_size.x_axis;
  precise_box.desired[Axis::Y] = precise_box_size.y_axis;
  
  al.calculate_standalone(precise_box);
  
  REQUIRE(precise_box.computed[Axis::X] == Approx(18.5f));
  REQUIRE(precise_box.computed[Axis::Y] == Approx(18.5f));
}

} // namespace component_tests
} // namespace ui
} // namespace afterhours