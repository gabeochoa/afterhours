#pragma once

#include "../../../../vendor/catch2/catch.hpp"
#include "../../../../src/plugins/autolayout.h"

namespace afterhours {
namespace ui {
namespace component_tests {

TEST_CASE("CheckboxNoLabelComponentSizeBasic", "[AutoLayout][ComponentSize][CheckboxNoLabel]") {
  // Test basic CheckboxNoLabel component autolayout
  AutoLayout al; // Empty mapping
  
  // Create a UIComponent representing a CheckboxNoLabel
  UIComponent checkbox_no_label_cmp(1);
  
  // Standard checkbox box only (no label text)
  ComponentSize checkbox_size = ComponentSize{pixels(20.f), pixels(20.f)};
  
  // Apply the ComponentSize to the UIComponent's desired sizes
  checkbox_no_label_cmp.desired[Axis::X] = checkbox_size.x_axis;
  checkbox_no_label_cmp.desired[Axis::Y] = checkbox_size.y_axis;
  
  // Verify the size was set correctly with pixels dimension
  REQUIRE(checkbox_no_label_cmp.desired[Axis::X].dim == Dim::Pixels);
  REQUIRE(checkbox_no_label_cmp.desired[Axis::Y].dim == Dim::Pixels);
  REQUIRE(checkbox_no_label_cmp.desired[Axis::X].value == 20.f);
  REQUIRE(checkbox_no_label_cmp.desired[Axis::Y].value == 20.f);
  
  // Run autolayout calculations
  al.calculate_standalone(checkbox_no_label_cmp);
  
  // Verify computed values match the pixels() settings
  REQUIRE(checkbox_no_label_cmp.computed[Axis::X] == 20.f);
  REQUIRE(checkbox_no_label_cmp.computed[Axis::Y] == 20.f);
}

TEST_CASE("CheckboxNoLabelComponentSizeVariants", "[AutoLayout][ComponentSize][CheckboxNoLabel]") {
  // Test CheckboxNoLabel component with various box sizes
  AutoLayout al; // Empty mapping
  
  // Test tiny checkbox (minimal UI)
  UIComponent tiny_checkbox(1);
  ComponentSize tiny_size = ComponentSize{pixels(12.f), pixels(12.f)};
  tiny_checkbox.desired[Axis::X] = tiny_size.x_axis;
  tiny_checkbox.desired[Axis::Y] = tiny_size.y_axis;
  
  al.calculate_standalone(tiny_checkbox);
  
  REQUIRE(tiny_checkbox.computed[Axis::X] == 12.f);
  REQUIRE(tiny_checkbox.computed[Axis::Y] == 12.f);
  
  // Test small checkbox (compact UI)
  UIComponent small_checkbox(2);
  ComponentSize small_size = ComponentSize{pixels(16.f), pixels(16.f)};
  small_checkbox.desired[Axis::X] = small_size.x_axis;
  small_checkbox.desired[Axis::Y] = small_size.y_axis;
  
  al.calculate_standalone(small_checkbox);
  
  REQUIRE(small_checkbox.computed[Axis::X] == 16.f);
  REQUIRE(small_checkbox.computed[Axis::Y] == 16.f);
  
  // Test standard checkbox (most common)
  UIComponent standard_checkbox(3);
  ComponentSize standard_size = ComponentSize{pixels(20.f), pixels(20.f)};
  standard_checkbox.desired[Axis::X] = standard_size.x_axis;
  standard_checkbox.desired[Axis::Y] = standard_size.y_axis;
  
  al.calculate_standalone(standard_checkbox);
  
  REQUIRE(standard_checkbox.computed[Axis::X] == 20.f);
  REQUIRE(standard_checkbox.computed[Axis::Y] == 20.f);
  
  // Test large checkbox (accessibility/touch-friendly)
  UIComponent large_checkbox(4);
  ComponentSize large_size = ComponentSize{pixels(24.f), pixels(24.f)};
  large_checkbox.desired[Axis::X] = large_size.x_axis;
  large_checkbox.desired[Axis::Y] = large_size.y_axis;
  
  al.calculate_standalone(large_checkbox);
  
  REQUIRE(large_checkbox.computed[Axis::X] == 24.f);
  REQUIRE(large_checkbox.computed[Axis::Y] == 24.f);
  
  // Test extra large checkbox (high DPI/accessibility)
  UIComponent xl_checkbox(5);
  ComponentSize xl_size = ComponentSize{pixels(32.f), pixels(32.f)};
  xl_checkbox.desired[Axis::X] = xl_size.x_axis;
  xl_checkbox.desired[Axis::Y] = xl_size.y_axis;
  
  al.calculate_standalone(xl_checkbox);
  
  REQUIRE(xl_checkbox.computed[Axis::X] == 32.f);
  REQUIRE(xl_checkbox.computed[Axis::Y] == 32.f);
}

TEST_CASE("CheckboxNoLabelComponentSizeSpecialCases", "[AutoLayout][ComponentSize][CheckboxNoLabel]") {
  // Test CheckboxNoLabel component for special use cases
  AutoLayout al; // Empty mapping
  
  // Test list item checkbox (table/grid selection)
  UIComponent list_checkbox(1);
  ComponentSize list_size = ComponentSize{pixels(18.f), pixels(18.f)};
  list_checkbox.desired[Axis::X] = list_size.x_axis;
  list_checkbox.desired[Axis::Y] = list_size.y_axis;
  
  al.calculate_standalone(list_checkbox);
  
  REQUIRE(list_checkbox.computed[Axis::X] == 18.f);
  REQUIRE(list_checkbox.computed[Axis::Y] == 18.f);
  
  // Test toolbar checkbox (compact toolbar)
  UIComponent toolbar_checkbox(2);
  ComponentSize toolbar_size = ComponentSize{pixels(14.f), pixels(14.f)};
  toolbar_checkbox.desired[Axis::X] = toolbar_size.x_axis;
  toolbar_checkbox.desired[Axis::Y] = toolbar_size.y_axis;
  
  al.calculate_standalone(toolbar_checkbox);
  
  REQUIRE(toolbar_checkbox.computed[Axis::X] == 14.f);
  REQUIRE(toolbar_checkbox.computed[Axis::Y] == 14.f);
  
  // Test menu checkbox (context menu)
  UIComponent menu_checkbox(3);
  ComponentSize menu_size = ComponentSize{pixels(16.f), pixels(16.f)};
  menu_checkbox.desired[Axis::X] = menu_size.x_axis;
  menu_checkbox.desired[Axis::Y] = menu_size.y_axis;
  
  al.calculate_standalone(menu_checkbox);
  
  REQUIRE(menu_checkbox.computed[Axis::X] == 16.f);
  REQUIRE(menu_checkbox.computed[Axis::Y] == 16.f);
  
  // Test toggle switch styled checkbox
  UIComponent toggle_checkbox(4);
  ComponentSize toggle_size = ComponentSize{pixels(40.f), pixels(20.f)};
  toggle_checkbox.desired[Axis::X] = toggle_size.x_axis;
  toggle_checkbox.desired[Axis::Y] = toggle_size.y_axis;
  
  al.calculate_standalone(toggle_checkbox);
  
  REQUIRE(toggle_checkbox.computed[Axis::X] == 40.f);
  REQUIRE(toggle_checkbox.computed[Axis::Y] == 20.f);
}

TEST_CASE("CheckboxNoLabelComponentSizeGridArray", "[AutoLayout][ComponentSize][CheckboxNoLabel]") {
  // Test CheckboxNoLabel component for grid/array layouts
  AutoLayout al; // Empty mapping
  
  // Test data table checkbox (consistent grid)
  UIComponent table_checkbox(1);
  ComponentSize table_size = ComponentSize{pixels(22.f), pixels(22.f)};
  table_checkbox.desired[Axis::X] = table_size.x_axis;
  table_checkbox.desired[Axis::Y] = table_size.y_axis;
  
  al.calculate_standalone(table_checkbox);
  
  REQUIRE(table_checkbox.computed[Axis::X] == 22.f);
  REQUIRE(table_checkbox.computed[Axis::Y] == 22.f);
  
  // Test card grid checkbox (card selection)
  UIComponent card_checkbox(2);
  ComponentSize card_size = ComponentSize{pixels(26.f), pixels(26.f)};
  card_checkbox.desired[Axis::X] = card_size.x_axis;
  card_checkbox.desired[Axis::Y] = card_size.y_axis;
  
  al.calculate_standalone(card_checkbox);
  
  REQUIRE(card_checkbox.computed[Axis::X] == 26.f);
  REQUIRE(card_checkbox.computed[Axis::Y] == 26.f);
  
  // Test thumbnail grid checkbox
  UIComponent thumb_checkbox(3);
  ComponentSize thumb_size = ComponentSize{pixels(24.f), pixels(24.f)};
  thumb_checkbox.desired[Axis::X] = thumb_size.x_axis;
  thumb_checkbox.desired[Axis::Y] = thumb_size.y_axis;
  
  al.calculate_standalone(thumb_checkbox);
  
  REQUIRE(thumb_checkbox.computed[Axis::X] == 24.f);
  REQUIRE(thumb_checkbox.computed[Axis::Y] == 24.f);
}

TEST_CASE("CheckboxNoLabelComponentSizePrecision", "[AutoLayout][ComponentSize][CheckboxNoLabel]") {
  // Test CheckboxNoLabel component with fractional pixel values
  AutoLayout al; // Empty mapping
  
  UIComponent precise_checkbox(1);
  ComponentSize precise_size = ComponentSize{pixels(19.5f), pixels(19.5f)};
  precise_checkbox.desired[Axis::X] = precise_size.x_axis;
  precise_checkbox.desired[Axis::Y] = precise_size.y_axis;
  
  al.calculate_standalone(precise_checkbox);
  
  REQUIRE(precise_checkbox.computed[Axis::X] == Approx(19.5f));
  REQUIRE(precise_checkbox.computed[Axis::Y] == Approx(19.5f));
  
  // Test very precise fractional checkbox
  UIComponent micro_precise(2);
  ComponentSize micro_size = ComponentSize{pixels(15.25f), pixels(15.25f)};
  micro_precise.desired[Axis::X] = micro_size.x_axis;
  micro_precise.desired[Axis::Y] = micro_size.y_axis;
  
  al.calculate_standalone(micro_precise);
  
  REQUIRE(micro_precise.computed[Axis::X] == Approx(15.25f));
  REQUIRE(micro_precise.computed[Axis::Y] == Approx(15.25f));
  
  // Test toggle switch precise sizing
  UIComponent precise_toggle(3);
  ComponentSize toggle_precise_size = ComponentSize{pixels(42.75f), pixels(21.25f)};
  precise_toggle.desired[Axis::X] = toggle_precise_size.x_axis;
  precise_toggle.desired[Axis::Y] = toggle_precise_size.y_axis;
  
  al.calculate_standalone(precise_toggle);
  
  REQUIRE(precise_toggle.computed[Axis::X] == Approx(42.75f));
  REQUIRE(precise_toggle.computed[Axis::Y] == Approx(21.25f));
}

} // namespace component_tests
} // namespace ui
} // namespace afterhours