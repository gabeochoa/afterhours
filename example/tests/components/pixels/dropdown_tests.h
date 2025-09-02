#pragma once

#include "../../../../vendor/catch2/catch.hpp"
#include "../../../../src/plugins/autolayout.h"

namespace afterhours {
namespace ui {
namespace component_tests {

TEST_CASE("DropdownComponentSizeBasic", "[AutoLayout][ComponentSize][Dropdown]") {
  // Test basic Dropdown component autolayout
  AutoLayout al; // Empty mapping
  
  // Create a UIComponent representing a Dropdown
  UIComponent dropdown_cmp(1);
  
  // Standard dropdown size (similar to button but wider for text)
  ComponentSize dropdown_size = ComponentSize{pixels(180.f), pixels(32.f)};
  
  // Apply the ComponentSize to the UIComponent's desired sizes
  dropdown_cmp.desired[Axis::X] = dropdown_size.x_axis;
  dropdown_cmp.desired[Axis::Y] = dropdown_size.y_axis;
  
  // Verify the size was set correctly with pixels dimension
  REQUIRE(dropdown_cmp.desired[Axis::X].dim == Dim::Pixels);
  REQUIRE(dropdown_cmp.desired[Axis::Y].dim == Dim::Pixels);
  REQUIRE(dropdown_cmp.desired[Axis::X].value == 180.f);
  REQUIRE(dropdown_cmp.desired[Axis::Y].value == 32.f);
  
  // Run autolayout calculations
  al.calculate_standalone(dropdown_cmp);
  
  // Verify computed values match the pixels() settings
  REQUIRE(dropdown_cmp.computed[Axis::X] == 180.f);
  REQUIRE(dropdown_cmp.computed[Axis::Y] == 32.f);
}

TEST_CASE("DropdownComponentSizeVariants", "[AutoLayout][ComponentSize][Dropdown]") {
  // Test Dropdown component with various typical sizes
  AutoLayout al; // Empty mapping
  
  // Test compact dropdown (toolbar/small form)
  UIComponent compact_dropdown(1);
  ComponentSize compact_size = ComponentSize{pixels(120.f), pixels(28.f)};
  compact_dropdown.desired[Axis::X] = compact_size.x_axis;
  compact_dropdown.desired[Axis::Y] = compact_size.y_axis;
  
  al.calculate_standalone(compact_dropdown);
  
  REQUIRE(compact_dropdown.computed[Axis::X] == 120.f);
  REQUIRE(compact_dropdown.computed[Axis::Y] == 28.f);
  
  // Test standard dropdown (most common)
  UIComponent standard_dropdown(2);
  ComponentSize standard_size = ComponentSize{pixels(160.f), pixels(32.f)};
  standard_dropdown.desired[Axis::X] = standard_size.x_axis;
  standard_dropdown.desired[Axis::Y] = standard_size.y_axis;
  
  al.calculate_standalone(standard_dropdown);
  
  REQUIRE(standard_dropdown.computed[Axis::X] == 160.f);
  REQUIRE(standard_dropdown.computed[Axis::Y] == 32.f);
  
  // Test wide dropdown (long options)
  UIComponent wide_dropdown(3);
  ComponentSize wide_size = ComponentSize{pixels(250.f), pixels(36.f)};
  wide_dropdown.desired[Axis::X] = wide_size.x_axis;
  wide_dropdown.desired[Axis::Y] = wide_size.y_axis;
  
  al.calculate_standalone(wide_dropdown);
  
  REQUIRE(wide_dropdown.computed[Axis::X] == 250.f);
  REQUIRE(wide_dropdown.computed[Axis::Y] == 36.f);
  
  // Test full-width dropdown (responsive form)
  UIComponent fullwidth_dropdown(4);
  ComponentSize fullwidth_size = ComponentSize{pixels(320.f), pixels(40.f)};
  fullwidth_dropdown.desired[Axis::X] = fullwidth_size.x_axis;
  fullwidth_dropdown.desired[Axis::Y] = fullwidth_size.y_axis;
  
  al.calculate_standalone(fullwidth_dropdown);
  
  REQUIRE(fullwidth_dropdown.computed[Axis::X] == 320.f);
  REQUIRE(fullwidth_dropdown.computed[Axis::Y] == 40.f);
}

TEST_CASE("DropdownComponentSizeSpecializedTypes", "[AutoLayout][ComponentSize][Dropdown]") {
  // Test specialized dropdown types with appropriate sizes
  AutoLayout al; // Empty mapping
  
  // Test select dropdown (form field)
  UIComponent select_dropdown(1);
  ComponentSize select_size = ComponentSize{pixels(200.f), pixels(34.f)};
  select_dropdown.desired[Axis::X] = select_size.x_axis;
  select_dropdown.desired[Axis::Y] = select_size.y_axis;
  
  al.calculate_standalone(select_dropdown);
  
  REQUIRE(select_dropdown.computed[Axis::X] == 200.f);
  REQUIRE(select_dropdown.computed[Axis::Y] == 34.f);
  
  // Test combobox dropdown (searchable)
  UIComponent combobox_dropdown(2);
  ComponentSize combobox_size = ComponentSize{pixels(220.f), pixels(36.f)};
  combobox_dropdown.desired[Axis::X] = combobox_size.x_axis;
  combobox_dropdown.desired[Axis::Y] = combobox_size.y_axis;
  
  al.calculate_standalone(combobox_dropdown);
  
  REQUIRE(combobox_dropdown.computed[Axis::X] == 220.f);
  REQUIRE(combobox_dropdown.computed[Axis::Y] == 36.f);
  
  // Test menu dropdown (navigation)
  UIComponent menu_dropdown(3);
  ComponentSize menu_size = ComponentSize{pixels(140.f), pixels(30.f)};
  menu_dropdown.desired[Axis::X] = menu_size.x_axis;
  menu_dropdown.desired[Axis::Y] = menu_size.y_axis;
  
  al.calculate_standalone(menu_dropdown);
  
  REQUIRE(menu_dropdown.computed[Axis::X] == 140.f);
  REQUIRE(menu_dropdown.computed[Axis::Y] == 30.f);
  
  // Test filter dropdown (data table)
  UIComponent filter_dropdown(4);
  ComponentSize filter_size = ComponentSize{pixels(100.f), pixels(26.f)};
  filter_dropdown.desired[Axis::X] = filter_size.x_axis;
  filter_dropdown.desired[Axis::Y] = filter_size.y_axis;
  
  al.calculate_standalone(filter_dropdown);
  
  REQUIRE(filter_dropdown.computed[Axis::X] == 100.f);
  REQUIRE(filter_dropdown.computed[Axis::Y] == 26.f);
}

TEST_CASE("DropdownComponentSizeFormContexts", "[AutoLayout][ComponentSize][Dropdown]") {
  // Test Dropdown component sizes for different form contexts
  AutoLayout al; // Empty mapping
  
  // Test inline form dropdown
  UIComponent inline_dropdown(1);
  ComponentSize inline_size = ComponentSize{pixels(130.f), pixels(28.f)};
  inline_dropdown.desired[Axis::X] = inline_size.x_axis;
  inline_dropdown.desired[Axis::Y] = inline_size.y_axis;
  
  al.calculate_standalone(inline_dropdown);
  
  REQUIRE(inline_dropdown.computed[Axis::X] == 130.f);
  REQUIRE(inline_dropdown.computed[Axis::Y] == 28.f);
  
  // Test vertical form dropdown
  UIComponent vertical_dropdown(2);
  ComponentSize vertical_size = ComponentSize{pixels(280.f), pixels(38.f)};
  vertical_dropdown.desired[Axis::X] = vertical_size.x_axis;
  vertical_dropdown.desired[Axis::Y] = vertical_size.y_axis;
  
  al.calculate_standalone(vertical_dropdown);
  
  REQUIRE(vertical_dropdown.computed[Axis::X] == 280.f);
  REQUIRE(vertical_dropdown.computed[Axis::Y] == 38.f);
  
  // Test grid form dropdown
  UIComponent grid_dropdown(3);
  ComponentSize grid_size = ComponentSize{pixels(190.f), pixels(34.f)};
  grid_dropdown.desired[Axis::X] = grid_size.x_axis;
  grid_dropdown.desired[Axis::Y] = grid_size.y_axis;
  
  al.calculate_standalone(grid_dropdown);
  
  REQUIRE(grid_dropdown.computed[Axis::X] == 190.f);
  REQUIRE(grid_dropdown.computed[Axis::Y] == 34.f);
  
  // Test settings panel dropdown
  UIComponent settings_dropdown(4);
  ComponentSize settings_size = ComponentSize{pixels(240.f), pixels(36.f)};
  settings_dropdown.desired[Axis::X] = settings_size.x_axis;
  settings_dropdown.desired[Axis::Y] = settings_size.y_axis;
  
  al.calculate_standalone(settings_dropdown);
  
  REQUIRE(settings_dropdown.computed[Axis::X] == 240.f);
  REQUIRE(settings_dropdown.computed[Axis::Y] == 36.f);
}

TEST_CASE("DropdownComponentSizeAccessibility", "[AutoLayout][ComponentSize][Dropdown]") {
  // Test Dropdown component sizes for accessibility requirements
  AutoLayout al; // Empty mapping
  
  // Test touch-friendly dropdown (mobile)
  UIComponent touch_dropdown(1);
  ComponentSize touch_size = ComponentSize{pixels(200.f), pixels(44.f)};
  touch_dropdown.desired[Axis::X] = touch_size.x_axis;
  touch_dropdown.desired[Axis::Y] = touch_size.y_axis;
  
  al.calculate_standalone(touch_dropdown);
  
  REQUIRE(touch_dropdown.computed[Axis::X] == 200.f);
  REQUIRE(touch_dropdown.computed[Axis::Y] == 44.f);
  
  // Test high contrast dropdown (larger text)
  UIComponent hc_dropdown(2);
  ComponentSize hc_size = ComponentSize{pixels(220.f), pixels(42.f)};
  hc_dropdown.desired[Axis::X] = hc_size.x_axis;
  hc_dropdown.desired[Axis::Y] = hc_size.y_axis;
  
  al.calculate_standalone(hc_dropdown);
  
  REQUIRE(hc_dropdown.computed[Axis::X] == 220.f);
  REQUIRE(hc_dropdown.computed[Axis::Y] == 42.f);
  
  // Test large text dropdown (vision accessibility)
  UIComponent large_dropdown(3);
  ComponentSize large_size = ComponentSize{pixels(260.f), pixels(48.f)};
  large_dropdown.desired[Axis::X] = large_size.x_axis;
  large_dropdown.desired[Axis::Y] = large_size.y_axis;
  
  al.calculate_standalone(large_dropdown);
  
  REQUIRE(large_dropdown.computed[Axis::X] == 260.f);
  REQUIRE(large_dropdown.computed[Axis::Y] == 48.f);
}

TEST_CASE("DropdownComponentSizePrecision", "[AutoLayout][ComponentSize][Dropdown]") {
  // Test Dropdown component with fractional pixel values
  AutoLayout al; // Empty mapping
  
  UIComponent precise_dropdown(1);
  ComponentSize precise_size = ComponentSize{pixels(167.75f), pixels(31.25f)};
  precise_dropdown.desired[Axis::X] = precise_size.x_axis;
  precise_dropdown.desired[Axis::Y] = precise_size.y_axis;
  
  al.calculate_standalone(precise_dropdown);
  
  REQUIRE(precise_dropdown.computed[Axis::X] == Approx(167.75f));
  REQUIRE(precise_dropdown.computed[Axis::Y] == Approx(31.25f));
  
  // Test very precise dropdown sizing
  UIComponent micro_dropdown(2);
  ComponentSize micro_size = ComponentSize{pixels(125.5f), pixels(27.75f)};
  micro_dropdown.desired[Axis::X] = micro_size.x_axis;
  micro_dropdown.desired[Axis::Y] = micro_size.y_axis;
  
  al.calculate_standalone(micro_dropdown);
  
  REQUIRE(micro_dropdown.computed[Axis::X] == Approx(125.5f));
  REQUIRE(micro_dropdown.computed[Axis::Y] == Approx(27.75f));
}

} // namespace component_tests
} // namespace ui
} // namespace afterhours