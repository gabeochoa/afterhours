#pragma once
#include "../../../../vendor/catch2/catch.hpp"
#include "../../../../src/plugins/autolayout.h"
namespace afterhours { namespace ui { namespace component_tests { namespace percent_tests {

TEST_CASE("ButtonGroupComponentPercentBasic", "[AutoLayout][ComponentSize][ButtonGroup][Percent]") {
  AutoLayout al;
  UIComponent button_group_cmp(1);
  ComponentSize button_group_size = ComponentSize{percent(0.8f), percent(0.15f)};
  button_group_cmp.desired[Axis::X] = button_group_size.x_axis;
  button_group_cmp.desired[Axis::Y] = button_group_size.y_axis;
  REQUIRE(button_group_cmp.desired[Axis::X].dim == Dim::Percent);
  REQUIRE(button_group_cmp.desired[Axis::Y].dim == Dim::Percent);
  REQUIRE(button_group_cmp.desired[Axis::X].value == 0.8f);
  REQUIRE(button_group_cmp.desired[Axis::Y].value == 0.15f);
  al.calculate_standalone(button_group_cmp);
  REQUIRE(button_group_cmp.computed[Axis::X] == -1.f);
  REQUIRE(button_group_cmp.computed[Axis::Y] == -1.f);
}

}}}} // namespaces