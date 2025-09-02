#pragma once
#include "../../../../vendor/catch2/catch.hpp"
#include "../../../../src/plugins/autolayout.h"
namespace afterhours { namespace ui { namespace component_tests { namespace percent_tests {

TEST_CASE("DropdownComponentPercentBasic", "[AutoLayout][ComponentSize][Dropdown][Percent]") {
  AutoLayout al;
  UIComponent dropdown_cmp(1);
  ComponentSize dropdown_size = ComponentSize{percent(0.4f), percent(0.06f)};
  dropdown_cmp.desired[Axis::X] = dropdown_size.x_axis;
  dropdown_cmp.desired[Axis::Y] = dropdown_size.y_axis;
  REQUIRE(dropdown_cmp.desired[Axis::X].dim == Dim::Percent);
  REQUIRE(dropdown_cmp.desired[Axis::Y].dim == Dim::Percent);
  REQUIRE(dropdown_cmp.desired[Axis::X].value == 0.4f);
  REQUIRE(dropdown_cmp.desired[Axis::Y].value == 0.06f);
  al.calculate_standalone(dropdown_cmp);
  REQUIRE(dropdown_cmp.computed[Axis::X] == -1.f);
  REQUIRE(dropdown_cmp.computed[Axis::Y] == -1.f);
}

}}}} // namespaces