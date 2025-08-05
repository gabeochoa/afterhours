

#define CATCH_CONFIG_MAIN
#include "../../vendor/catch2/catch.hpp"
#include "../../vendor/trompeloeil/trompeloeil.hpp"
//
#include "../../src/plugins/autolayout.h"

namespace afterhours {
namespace ui {
TEST_CASE("UIComponentTest", "[UIComponent]") {
  UIComponent cmp(-1);
  REQUIRE(cmp.flex_direction == FlexDirection::Column);
  REQUIRE_FALSE(cmp.was_rendered_to_screen);
  REQUIRE_FALSE(cmp.absolute);
}

TEST_CASE("RectCalculations", "[UIComponent]") {
  UIComponent cmp(0);
  cmp.computed[Axis::X] = 100.f;
  cmp.computed[Axis::Y] = 200.f;
  cmp.computed_rel[Axis::X] = 10.f;
  cmp.computed_rel[Axis::Y] = 20.f;

  Rectangle rect = cmp.rect();
  REQUIRE(rect.x == 10.f);
  REQUIRE(rect.y == 20.f);
  REQUIRE(rect.width == 100.f);
  REQUIRE(rect.height == 200.f);
}

TEST_CASE("AddRemoveChild", "[UIComponent]") {
  UIComponent parent(0);
  UIComponent child1(1);
  UIComponent child2(2);

  parent.add_child(child1.id);
  parent.add_child(child2.id);

  REQUIRE(parent.children.size() == 2);
  REQUIRE(parent.children[0] == child1.id);
  REQUIRE(parent.children[1] == child2.id);

  parent.remove_child(child1.id);

  REQUIRE(parent.children.size() == 1);
  REQUIRE(parent.children[0] == child2.id);
}

TEST_CASE("SetParent", "[UIComponent]") {
  UIComponent parent(1);
  UIComponent child(2);

  child.set_parent(parent.id);

  REQUIRE(child.parent == parent.id);
}

TEST_CASE("CalculateStandalone", "[AutoLayout]") {
  AutoLayout al; // Empty mapping
  UIComponent cmp(1);
  cmp.desired[Axis::X] = Size{Dim::Pixels, 100.f};
  cmp.desired[Axis::Y] = Size{Dim::Pixels, 200.f};

  al.calculate_standalone(cmp);

  REQUIRE(cmp.computed[Axis::X] == 100.f);
  REQUIRE(cmp.computed[Axis::Y] == 200.f);
}

TEST_CASE("AutoLayoutCalculateStandaloneWithPercent", "[AutoLayout]") {
  AutoLayout al; // Empty mapping

  UIComponent cmp(1);
  cmp.desired[Axis::X] = Size{Dim::Percent, 0.5f};
  cmp.desired[Axis::Y] = Size{Dim::Pixels, 200.f};

  al.calculate_standalone(cmp);

  // TODO
  // REQUIRE(cmp.computed[Axis::X] == 0.f); // Should not change
  REQUIRE(cmp.computed[Axis::Y] == 200.f);
}

// TODO: Text dimension test causes segfault - needs investigation
/*
TEST_CASE("AutoLayoutCalculateStandaloneWithText", "[AutoLayout]") {
  AutoLayout al; // Empty mapping
  UIComponent cmp(1);
  cmp.desired[Axis::X] = Size{Dim::Text, 100.f};
  cmp.desired[Axis::Y] = Size{Dim::Pixels, 200.f};

  al.calculate_standalone(cmp);

  REQUIRE(cmp.computed[Axis::X] == 100.f); // Default value for text
  REQUIRE(cmp.computed[Axis::Y] == 200.f);
}
*/

/*
class AutoLayoutMock : public AutoLayout {
public:
  MAKE_MOCK1(to_cmp, UIComponent &(EntityID));
};

TEST_CASE("AutoLayoutCalculateThoseWithParents", "[AutoLayout]") {
  UIComponent parent(1);
  parent.desired[Axis::X] = Size{Dim::Pixels, 1000.f};
  parent.desired[Axis::Y] = Size{Dim::Pixels, 200.f};
  parent.computed[Axis::X] = 1000.f;
  parent.computed[Axis::Y] = 200.f;

  UIComponent child(2);
  child.parent = parent.id;
  child.desired[Axis::X] = Size{Dim::Percent, 0.5f};
  child.desired[Axis::Y] = Size{Dim::Pixels, 200.f};

  AutoLayoutMock al;
  ALLOW_CALL(al, to_cmp(parent.id)).LR_RETURN(std::ref(parent));
  ALLOW_CALL(al, to_cmp(child.id)).LR_RETURN(std::ref(child));

  al.calculate_those_with_parents(child);

  REQUIRE(child.computed[Axis::X] == 500.f);
  // still zero since its calculate standalone
  REQUIRE(child.computed[Axis::Y] == Approx(0.f).epsilon(0.1f));
}
*/

TEST_CASE("AutoLayoutWithComponentSizeButton", "[AutoLayout][ComponentSize]") {
  // Test autolayout functionality with ComponentSize for Button component using pixels()
  AutoLayout al; // Empty mapping
  
  // Create a UIComponent representing a Button
  UIComponent button_cmp(1);
  
  // Create ComponentSize with pixels() setting similar to Button defaults
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

TEST_CASE("AutoLayoutComponentSizePixelsVariousValues", "[AutoLayout][ComponentSize]") {
  // Test ComponentSize with different pixel values to ensure flexibility
  AutoLayout al; // Empty mapping
  
  // Test small button (like a close button)
  UIComponent small_button(1);
  ComponentSize small_size = ComponentSize{pixels(30.f), pixels(30.f)};
  small_button.desired[Axis::X] = small_size.x_axis;
  small_button.desired[Axis::Y] = small_size.y_axis;
  
  al.calculate_standalone(small_button);
  
  REQUIRE(small_button.computed[Axis::X] == 30.f);
  REQUIRE(small_button.computed[Axis::Y] == 30.f);
  
  // Test large button (like a primary action button)
  UIComponent large_button(2);
  ComponentSize large_size = ComponentSize{pixels(200.f), pixels(60.f)};
  large_button.desired[Axis::X] = large_size.x_axis;
  large_button.desired[Axis::Y] = large_size.y_axis;
  
  al.calculate_standalone(large_button);
  
  REQUIRE(large_button.computed[Axis::X] == 200.f);
  REQUIRE(large_button.computed[Axis::Y] == 60.f);
  
  // Test with fractional pixels (for precision)
  UIComponent precise_button(3);
  ComponentSize precise_size = ComponentSize{pixels(125.5f), pixels(42.25f)};
  precise_button.desired[Axis::X] = precise_size.x_axis;
  precise_button.desired[Axis::Y] = precise_size.y_axis;
  
  al.calculate_standalone(precise_button);
  
  REQUIRE(precise_button.computed[Axis::X] == Approx(125.5f));
  REQUIRE(precise_button.computed[Axis::Y] == Approx(42.25f));
}

} // namespace ui
} // namespace afterhours
