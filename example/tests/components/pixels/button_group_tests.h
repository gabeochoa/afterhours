#pragma once

#include "../../../../vendor/catch2/catch.hpp"
#include "../../../../src/plugins/autolayout.h"

namespace afterhours {
namespace ui {
namespace component_tests {

TEST_CASE("ButtonGroupComponentSizeBasic", "[AutoLayout][ComponentSize][ButtonGroup]") {
  // Test basic ButtonGroup component autolayout
  AutoLayout al; // Empty mapping
  
  // Create a UIComponent representing a ButtonGroup
  UIComponent button_group_cmp(1);
  
  // ButtonGroup typically larger to contain multiple buttons
  // Standard horizontal button group size
  ComponentSize button_group_size = ComponentSize{pixels(300.f), pixels(40.f)};
  
  // Apply the ComponentSize to the UIComponent's desired sizes
  button_group_cmp.desired[Axis::X] = button_group_size.x_axis;
  button_group_cmp.desired[Axis::Y] = button_group_size.y_axis;
  
  // Verify the size was set correctly with pixels dimension
  REQUIRE(button_group_cmp.desired[Axis::X].dim == Dim::Pixels);
  REQUIRE(button_group_cmp.desired[Axis::Y].dim == Dim::Pixels);
  REQUIRE(button_group_cmp.desired[Axis::X].value == 300.f);
  REQUIRE(button_group_cmp.desired[Axis::Y].value == 40.f);
  
  // Run autolayout calculations
  al.calculate_standalone(button_group_cmp);
  
  // Verify computed values match the pixels() settings
  REQUIRE(button_group_cmp.computed[Axis::X] == 300.f);
  REQUIRE(button_group_cmp.computed[Axis::Y] == 40.f);
}

TEST_CASE("ButtonGroupComponentSizeVariants", "[AutoLayout][ComponentSize][ButtonGroup]") {
  // Test ButtonGroup component with various typical configurations
  AutoLayout al; // Empty mapping
  
  // Test compact button group (2-3 small buttons)
  UIComponent compact_group(1);
  ComponentSize compact_size = ComponentSize{pixels(150.f), pixels(32.f)};
  compact_group.desired[Axis::X] = compact_size.x_axis;
  compact_group.desired[Axis::Y] = compact_size.y_axis;
  
  al.calculate_standalone(compact_group);
  
  REQUIRE(compact_group.computed[Axis::X] == 150.f);
  REQUIRE(compact_group.computed[Axis::Y] == 32.f);
  
  // Test standard button group (3-4 medium buttons)
  UIComponent standard_group(2);
  ComponentSize standard_size = ComponentSize{pixels(280.f), pixels(40.f)};
  standard_group.desired[Axis::X] = standard_size.x_axis;
  standard_group.desired[Axis::Y] = standard_size.y_axis;
  
  al.calculate_standalone(standard_group);
  
  REQUIRE(standard_group.computed[Axis::X] == 280.f);
  REQUIRE(standard_group.computed[Axis::Y] == 40.f);
  
  // Test large button group (4-5 buttons or wide buttons)
  UIComponent large_group(3);
  ComponentSize large_size = ComponentSize{pixels(420.f), pixels(50.f)};
  large_group.desired[Axis::X] = large_size.x_axis;
  large_group.desired[Axis::Y] = large_size.y_axis;
  
  al.calculate_standalone(large_group);
  
  REQUIRE(large_group.computed[Axis::X] == 420.f);
  REQUIRE(large_group.computed[Axis::Y] == 50.f);
  
  // Test vertical button group (stacked buttons)
  UIComponent vertical_group(4);
  ComponentSize vertical_size = ComponentSize{pixels(120.f), pixels(140.f)};
  vertical_group.desired[Axis::X] = vertical_size.x_axis;
  vertical_group.desired[Axis::Y] = vertical_size.y_axis;
  
  al.calculate_standalone(vertical_group);
  
  REQUIRE(vertical_group.computed[Axis::X] == 120.f);
  REQUIRE(vertical_group.computed[Axis::Y] == 140.f);
}

TEST_CASE("ButtonGroupComponentSizeToolbar", "[AutoLayout][ComponentSize][ButtonGroup]") {
  // Test ButtonGroup component sized for toolbar scenarios
  AutoLayout al; // Empty mapping
  
  // Test icon toolbar (small icon buttons grouped)
  UIComponent icon_toolbar(1);
  ComponentSize icon_size = ComponentSize{pixels(160.f), pixels(28.f)};
  icon_toolbar.desired[Axis::X] = icon_size.x_axis;
  icon_toolbar.desired[Axis::Y] = icon_size.y_axis;
  
  al.calculate_standalone(icon_toolbar);
  
  REQUIRE(icon_toolbar.computed[Axis::X] == 160.f);
  REQUIRE(icon_toolbar.computed[Axis::Y] == 28.f);
  
  // Test full-width toolbar
  UIComponent full_toolbar(2);
  ComponentSize full_size = ComponentSize{pixels(800.f), pixels(36.f)};
  full_toolbar.desired[Axis::X] = full_size.x_axis;
  full_toolbar.desired[Axis::Y] = full_size.y_axis;
  
  al.calculate_standalone(full_toolbar);
  
  REQUIRE(full_toolbar.computed[Axis::X] == 800.f);
  REQUIRE(full_toolbar.computed[Axis::Y] == 36.f);
  
  // Test floating action button group
  UIComponent fab_group(3);
  ComponentSize fab_size = ComponentSize{pixels(180.f), pixels(56.f)};
  fab_group.desired[Axis::X] = fab_size.x_axis;
  fab_group.desired[Axis::Y] = fab_size.y_axis;
  
  al.calculate_standalone(fab_group);
  
  REQUIRE(fab_group.computed[Axis::X] == 180.f);
  REQUIRE(fab_group.computed[Axis::Y] == 56.f);
}

TEST_CASE("ButtonGroupComponentSizePrecision", "[AutoLayout][ComponentSize][ButtonGroup]") {
  // Test ButtonGroup component with fractional pixel values
  AutoLayout al; // Empty mapping
  
  UIComponent precise_group(1);
  ComponentSize precise_size = ComponentSize{pixels(275.25f), pixels(38.75f)};
  precise_group.desired[Axis::X] = precise_size.x_axis;
  precise_group.desired[Axis::Y] = precise_size.y_axis;
  
  al.calculate_standalone(precise_group);
  
  REQUIRE(precise_group.computed[Axis::X] == Approx(275.25f));
  REQUIRE(precise_group.computed[Axis::Y] == Approx(38.75f));
}

} // namespace component_tests
} // namespace ui
} // namespace afterhours