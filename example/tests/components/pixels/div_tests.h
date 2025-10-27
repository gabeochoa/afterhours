#pragma once

#include "../../../../vendor/catch2/catch.hpp"
#include "../../../../src/plugins/autolayout.h"

namespace afterhours {
namespace ui {
namespace component_tests {

TEST_CASE("DivComponentSizeBasic", "[AutoLayout][ComponentSize][Div]") {
  // Test basic Div component autolayout
  AutoLayout al; // Empty mapping
  
  // Create a UIComponent representing a Div
  UIComponent div_cmp(1);
  
  // Div components are typically containers, so they're often larger
  // Standard container div size
  ComponentSize div_size = ComponentSize{pixels(400.f), pixels(300.f)};
  
  // Apply the ComponentSize to the UIComponent's desired sizes
  div_cmp.desired[Axis::X] = div_size.x_axis;
  div_cmp.desired[Axis::Y] = div_size.y_axis;
  
  // Verify the size was set correctly with pixels dimension
  REQUIRE(div_cmp.desired[Axis::X].dim == Dim::Pixels);
  REQUIRE(div_cmp.desired[Axis::Y].dim == Dim::Pixels);
  REQUIRE(div_cmp.desired[Axis::X].value == 400.f);
  REQUIRE(div_cmp.desired[Axis::Y].value == 300.f);
  
  // Run autolayout calculations
  al.calculate_standalone(div_cmp);
  
  // Verify computed values match the pixels() settings
  REQUIRE(div_cmp.computed[Axis::X] == 400.f);
  REQUIRE(div_cmp.computed[Axis::Y] == 300.f);
}

TEST_CASE("DivComponentSizeContainerVariants", "[AutoLayout][ComponentSize][Div]") {
  // Test Div component with various container sizes
  AutoLayout al; // Empty mapping
  
  // Test small container div (widget container)
  UIComponent small_div(1);
  ComponentSize small_size = ComponentSize{pixels(200.f), pixels(150.f)};
  small_div.desired[Axis::X] = small_size.x_axis;
  small_div.desired[Axis::Y] = small_size.y_axis;
  
  al.calculate_standalone(small_div);
  
  REQUIRE(small_div.computed[Axis::X] == 200.f);
  REQUIRE(small_div.computed[Axis::Y] == 150.f);
  
  // Test medium container div (panel/card)
  UIComponent medium_div(2);
  ComponentSize medium_size = ComponentSize{pixels(350.f), pixels(250.f)};
  medium_div.desired[Axis::X] = medium_size.x_axis;
  medium_div.desired[Axis::Y] = medium_size.y_axis;
  
  al.calculate_standalone(medium_div);
  
  REQUIRE(medium_div.computed[Axis::X] == 350.f);
  REQUIRE(medium_div.computed[Axis::Y] == 250.f);
  
  // Test large container div (main content area)
  UIComponent large_div(3);
  ComponentSize large_size = ComponentSize{pixels(800.f), pixels(600.f)};
  large_div.desired[Axis::X] = large_size.x_axis;
  large_div.desired[Axis::Y] = large_size.y_axis;
  
  al.calculate_standalone(large_div);
  
  REQUIRE(large_div.computed[Axis::X] == 800.f);
  REQUIRE(large_div.computed[Axis::Y] == 600.f);
  
  // Test full screen div
  UIComponent fullscreen_div(4);
  ComponentSize fullscreen_size = ComponentSize{pixels(1920.f), pixels(1080.f)};
  fullscreen_div.desired[Axis::X] = fullscreen_size.x_axis;
  fullscreen_div.desired[Axis::Y] = fullscreen_size.y_axis;
  
  al.calculate_standalone(fullscreen_div);
  
  REQUIRE(fullscreen_div.computed[Axis::X] == 1920.f);
  REQUIRE(fullscreen_div.computed[Axis::Y] == 1080.f);
}

TEST_CASE("DivComponentSizeLayoutVariants", "[AutoLayout][ComponentSize][Div]") {
  // Test Div component with various layout-specific sizes
  AutoLayout al; // Empty mapping
  
  // Test sidebar div
  UIComponent sidebar_div(1);
  ComponentSize sidebar_size = ComponentSize{pixels(250.f), pixels(800.f)};
  sidebar_div.desired[Axis::X] = sidebar_size.x_axis;
  sidebar_div.desired[Axis::Y] = sidebar_size.y_axis;
  
  al.calculate_standalone(sidebar_div);
  
  REQUIRE(sidebar_div.computed[Axis::X] == 250.f);
  REQUIRE(sidebar_div.computed[Axis::Y] == 800.f);
  
  // Test header div
  UIComponent header_div(2);
  ComponentSize header_size = ComponentSize{pixels(1200.f), pixels(80.f)};
  header_div.desired[Axis::X] = header_size.x_axis;
  header_div.desired[Axis::Y] = header_size.y_axis;
  
  al.calculate_standalone(header_div);
  
  REQUIRE(header_div.computed[Axis::X] == 1200.f);
  REQUIRE(header_div.computed[Axis::Y] == 80.f);
  
  // Test footer div
  UIComponent footer_div(3);
  ComponentSize footer_size = ComponentSize{pixels(1200.f), pixels(60.f)};
  footer_div.desired[Axis::X] = footer_size.x_axis;
  footer_div.desired[Axis::Y] = footer_size.y_axis;
  
  al.calculate_standalone(footer_div);
  
  REQUIRE(footer_div.computed[Axis::X] == 1200.f);
  REQUIRE(footer_div.computed[Axis::Y] == 60.f);
  
  // Test modal div (overlay)
  UIComponent modal_div(4);
  ComponentSize modal_size = ComponentSize{pixels(500.f), pixels(400.f)};
  modal_div.desired[Axis::X] = modal_size.x_axis;
  modal_div.desired[Axis::Y] = modal_size.y_axis;
  
  al.calculate_standalone(modal_div);
  
  REQUIRE(modal_div.computed[Axis::X] == 500.f);
  REQUIRE(modal_div.computed[Axis::Y] == 400.f);
}

TEST_CASE("DivComponentSizeGridAndFlex", "[AutoLayout][ComponentSize][Div]") {
  // Test Div component sizes for grid and flexbox layouts
  AutoLayout al; // Empty mapping
  
  // Test grid cell div (1/3 width)
  UIComponent grid_cell(1);
  ComponentSize grid_size = ComponentSize{pixels(266.67f), pixels(200.f)};
  grid_cell.desired[Axis::X] = grid_size.x_axis;
  grid_cell.desired[Axis::Y] = grid_size.y_axis;
  
  al.calculate_standalone(grid_cell);
  
  REQUIRE(grid_cell.computed[Axis::X] == Approx(266.67f));
  REQUIRE(grid_cell.computed[Axis::Y] == 200.f);
  
  // Test flex item div (flex-grow)
  UIComponent flex_item(2);
  ComponentSize flex_size = ComponentSize{pixels(300.f), pixels(100.f)};
  flex_item.desired[Axis::X] = flex_size.x_axis;
  flex_item.desired[Axis::Y] = flex_size.y_axis;
  
  al.calculate_standalone(flex_item);
  
  REQUIRE(flex_item.computed[Axis::X] == 300.f);
  REQUIRE(flex_item.computed[Axis::Y] == 100.f);
  
  // Test square div (aspect ratio 1:1)
  UIComponent square_div(3);
  ComponentSize square_size = ComponentSize{pixels(250.f), pixels(250.f)};
  square_div.desired[Axis::X] = square_size.x_axis;
  square_div.desired[Axis::Y] = square_size.y_axis;
  
  al.calculate_standalone(square_div);
  
  REQUIRE(square_div.computed[Axis::X] == 250.f);
  REQUIRE(square_div.computed[Axis::Y] == 250.f);
}

TEST_CASE("DivComponentSizePrecision", "[AutoLayout][ComponentSize][Div]") {
  // Test Div component with fractional pixel values
  AutoLayout al; // Empty mapping
  
  UIComponent precise_div(1);
  ComponentSize precise_size = ComponentSize{pixels(384.5f), pixels(256.75f)};
  precise_div.desired[Axis::X] = precise_size.x_axis;
  precise_div.desired[Axis::Y] = precise_size.y_axis;
  
  al.calculate_standalone(precise_div);
  
  REQUIRE(precise_div.computed[Axis::X] == Approx(384.5f));
  REQUIRE(precise_div.computed[Axis::Y] == Approx(256.75f));
}

} // namespace component_tests
} // namespace ui
} // namespace afterhours