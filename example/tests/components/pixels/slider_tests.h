#pragma once

#include "../../../../vendor/catch2/catch.hpp"
#include "../../../../src/plugins/autolayout.h"

namespace afterhours {
namespace ui {
namespace component_tests {

TEST_CASE("SliderComponentSizeBasic", "[AutoLayout][ComponentSize][Slider]") {
  // Test basic Slider component autolayout
  AutoLayout al; // Empty mapping
  
  // Create a UIComponent representing a Slider
  UIComponent slider_cmp(1);
  
  // Standard horizontal slider size
  ComponentSize slider_size = ComponentSize{pixels(200.f), pixels(25.f)};
  
  // Apply the ComponentSize to the UIComponent's desired sizes
  slider_cmp.desired[Axis::X] = slider_size.x_axis;
  slider_cmp.desired[Axis::Y] = slider_size.y_axis;
  
  // Verify the size was set correctly with pixels dimension
  REQUIRE(slider_cmp.desired[Axis::X].dim == Dim::Pixels);
  REQUIRE(slider_cmp.desired[Axis::Y].dim == Dim::Pixels);
  REQUIRE(slider_cmp.desired[Axis::X].value == 200.f);
  REQUIRE(slider_cmp.desired[Axis::Y].value == 25.f);
  
  // Run autolayout calculations
  al.calculate_standalone(slider_cmp);
  
  // Verify computed values match the pixels() settings
  REQUIRE(slider_cmp.computed[Axis::X] == 200.f);
  REQUIRE(slider_cmp.computed[Axis::Y] == 25.f);
}

TEST_CASE("SliderComponentSizeHorizontalVariants", "[AutoLayout][ComponentSize][Slider]") {
  // Test Slider component with various horizontal slider sizes
  AutoLayout al; // Empty mapping
  
  // Test compact horizontal slider
  UIComponent compact_slider(1);
  ComponentSize compact_size = ComponentSize{pixels(120.f), pixels(20.f)};
  compact_slider.desired[Axis::X] = compact_size.x_axis;
  compact_slider.desired[Axis::Y] = compact_size.y_axis;
  
  al.calculate_standalone(compact_slider);
  
  REQUIRE(compact_slider.computed[Axis::X] == 120.f);
  REQUIRE(compact_slider.computed[Axis::Y] == 20.f);
  
  // Test standard horizontal slider
  UIComponent standard_slider(2);
  ComponentSize standard_size = ComponentSize{pixels(180.f), pixels(24.f)};
  standard_slider.desired[Axis::X] = standard_size.x_axis;
  standard_slider.desired[Axis::Y] = standard_size.y_axis;
  
  al.calculate_standalone(standard_slider);
  
  REQUIRE(standard_slider.computed[Axis::X] == 180.f);
  REQUIRE(standard_slider.computed[Axis::Y] == 24.f);
  
  // Test wide horizontal slider
  UIComponent wide_slider(3);
  ComponentSize wide_size = ComponentSize{pixels(300.f), pixels(28.f)};
  wide_slider.desired[Axis::X] = wide_size.x_axis;
  wide_slider.desired[Axis::Y] = wide_size.y_axis;
  
  al.calculate_standalone(wide_slider);
  
  REQUIRE(wide_slider.computed[Axis::X] == 300.f);
  REQUIRE(wide_slider.computed[Axis::Y] == 28.f);
  
  // Test full-width horizontal slider
  UIComponent fullwidth_slider(4);
  ComponentSize fullwidth_size = ComponentSize{pixels(500.f), pixels(32.f)};
  fullwidth_slider.desired[Axis::X] = fullwidth_size.x_axis;
  fullwidth_slider.desired[Axis::Y] = fullwidth_size.y_axis;
  
  al.calculate_standalone(fullwidth_slider);
  
  REQUIRE(fullwidth_slider.computed[Axis::X] == 500.f);
  REQUIRE(fullwidth_slider.computed[Axis::Y] == 32.f);
}

TEST_CASE("SliderComponentSizeVerticalVariants", "[AutoLayout][ComponentSize][Slider]") {
  // Test Slider component with various vertical slider sizes
  AutoLayout al; // Empty mapping
  
  // Test compact vertical slider
  UIComponent compact_vertical(1);
  ComponentSize compact_v_size = ComponentSize{pixels(20.f), pixels(120.f)};
  compact_vertical.desired[Axis::X] = compact_v_size.x_axis;
  compact_vertical.desired[Axis::Y] = compact_v_size.y_axis;
  
  al.calculate_standalone(compact_vertical);
  
  REQUIRE(compact_vertical.computed[Axis::X] == 20.f);
  REQUIRE(compact_vertical.computed[Axis::Y] == 120.f);
  
  // Test standard vertical slider
  UIComponent standard_vertical(2);
  ComponentSize standard_v_size = ComponentSize{pixels(24.f), pixels(180.f)};
  standard_vertical.desired[Axis::X] = standard_v_size.x_axis;
  standard_vertical.desired[Axis::Y] = standard_v_size.y_axis;
  
  al.calculate_standalone(standard_vertical);
  
  REQUIRE(standard_vertical.computed[Axis::X] == 24.f);
  REQUIRE(standard_vertical.computed[Axis::Y] == 180.f);
  
  // Test tall vertical slider
  UIComponent tall_vertical(3);
  ComponentSize tall_v_size = ComponentSize{pixels(28.f), pixels(300.f)};
  tall_vertical.desired[Axis::X] = tall_v_size.x_axis;
  tall_vertical.desired[Axis::Y] = tall_v_size.y_axis;
  
  al.calculate_standalone(tall_vertical);
  
  REQUIRE(tall_vertical.computed[Axis::X] == 28.f);
  REQUIRE(tall_vertical.computed[Axis::Y] == 300.f);
  
  // Test full-height vertical slider
  UIComponent fullheight_vertical(4);
  ComponentSize fullheight_v_size = ComponentSize{pixels(32.f), pixels(400.f)};
  fullheight_vertical.desired[Axis::X] = fullheight_v_size.x_axis;
  fullheight_vertical.desired[Axis::Y] = fullheight_v_size.y_axis;
  
  al.calculate_standalone(fullheight_vertical);
  
  REQUIRE(fullheight_vertical.computed[Axis::X] == 32.f);
  REQUIRE(fullheight_vertical.computed[Axis::Y] == 400.f);
}

TEST_CASE("SliderComponentSizeSpecializedTypes", "[AutoLayout][ComponentSize][Slider]") {
  // Test specialized slider types with appropriate sizes
  AutoLayout al; // Empty mapping
  
  // Test volume slider (compact)
  UIComponent volume_slider(1);
  ComponentSize volume_size = ComponentSize{pixels(100.f), pixels(20.f)};
  volume_slider.desired[Axis::X] = volume_size.x_axis;
  volume_slider.desired[Axis::Y] = volume_size.y_axis;
  
  al.calculate_standalone(volume_slider);
  
  REQUIRE(volume_slider.computed[Axis::X] == 100.f);
  REQUIRE(volume_slider.computed[Axis::Y] == 20.f);
  
  // Test progress slider (wide)
  UIComponent progress_slider(2);
  ComponentSize progress_size = ComponentSize{pixels(400.f), pixels(12.f)};
  progress_slider.desired[Axis::X] = progress_size.x_axis;
  progress_slider.desired[Axis::Y] = progress_size.y_axis;
  
  al.calculate_standalone(progress_slider);
  
  REQUIRE(progress_slider.computed[Axis::X] == 400.f);
  REQUIRE(progress_slider.computed[Axis::Y] == 12.f);
  
  // Test zoom slider (medium)
  UIComponent zoom_slider(3);
  ComponentSize zoom_size = ComponentSize{pixels(150.f), pixels(22.f)};
  zoom_slider.desired[Axis::X] = zoom_size.x_axis;
  zoom_slider.desired[Axis::Y] = zoom_size.y_axis;
  
  al.calculate_standalone(zoom_slider);
  
  REQUIRE(zoom_slider.computed[Axis::X] == 150.f);
  REQUIRE(zoom_slider.computed[Axis::Y] == 22.f);
  
  // Test range slider (dual thumb - wider)
  UIComponent range_slider(4);
  ComponentSize range_size = ComponentSize{pixels(250.f), pixels(30.f)};
  range_slider.desired[Axis::X] = range_size.x_axis;
  range_slider.desired[Axis::Y] = range_size.y_axis;
  
  al.calculate_standalone(range_slider);
  
  REQUIRE(range_slider.computed[Axis::X] == 250.f);
  REQUIRE(range_slider.computed[Axis::Y] == 30.f);
}

TEST_CASE("SliderComponentSizePrecision", "[AutoLayout][ComponentSize][Slider]") {
  // Test Slider component with fractional pixel values
  AutoLayout al; // Empty mapping
  
  UIComponent precise_slider(1);
  ComponentSize precise_size = ComponentSize{pixels(185.25f), pixels(23.75f)};
  precise_slider.desired[Axis::X] = precise_size.x_axis;
  precise_slider.desired[Axis::Y] = precise_size.y_axis;
  
  al.calculate_standalone(precise_slider);
  
  REQUIRE(precise_slider.computed[Axis::X] == Approx(185.25f));
  REQUIRE(precise_slider.computed[Axis::Y] == Approx(23.75f));
  
  // Test very precise vertical slider
  UIComponent precise_vertical(2);
  ComponentSize precise_v_size = ComponentSize{pixels(21.5f), pixels(156.25f)};
  precise_vertical.desired[Axis::X] = precise_v_size.x_axis;
  precise_vertical.desired[Axis::Y] = precise_v_size.y_axis;
  
  al.calculate_standalone(precise_vertical);
  
  REQUIRE(precise_vertical.computed[Axis::X] == Approx(21.5f));
  REQUIRE(precise_vertical.computed[Axis::Y] == Approx(156.25f));
}

} // namespace component_tests
} // namespace ui
} // namespace afterhours