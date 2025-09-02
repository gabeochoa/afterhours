#pragma once
#include "../../../../vendor/catch2/catch.hpp"
#include "../../../../src/plugins/autolayout.h"
namespace afterhours { namespace ui { namespace component_tests { namespace percent_tests {

TEST_CASE("CheckboxComponentPercentBasic", "[AutoLayout][ComponentSize][Checkbox][Percent]") {
  AutoLayout al;
  UIComponent checkbox_cmp(1);
  ComponentSize checkbox_size = ComponentSize{percent(0.3f), percent(0.05f)};
  checkbox_cmp.desired[Axis::X] = checkbox_size.x_axis;
  checkbox_cmp.desired[Axis::Y] = checkbox_size.y_axis;
  REQUIRE(checkbox_cmp.desired[Axis::X].dim == Dim::Percent);
  REQUIRE(checkbox_cmp.desired[Axis::Y].dim == Dim::Percent);
  REQUIRE(checkbox_cmp.desired[Axis::X].value == 0.3f);
  REQUIRE(checkbox_cmp.desired[Axis::Y].value == 0.05f);
  al.calculate_standalone(checkbox_cmp);
  REQUIRE(checkbox_cmp.computed[Axis::X] == -1.f);
  REQUIRE(checkbox_cmp.computed[Axis::Y] == -1.f);
}

}}}} // namespaces