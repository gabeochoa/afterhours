#pragma once
#include "../../../../vendor/catch2/catch.hpp"
#include "../../../../src/plugins/autolayout.h"
namespace afterhours { namespace ui { namespace component_tests { namespace percent_tests {

TEST_CASE("DivComponentPercentBasic", "[AutoLayout][ComponentSize][Div][Percent]") {
  AutoLayout al;
  UIComponent div_cmp(1);
  ComponentSize div_size = ComponentSize{percent(0.8f), percent(0.6f)};
  div_cmp.desired[Axis::X] = div_size.x_axis;
  div_cmp.desired[Axis::Y] = div_size.y_axis;
  REQUIRE(div_cmp.desired[Axis::X].dim == Dim::Percent);
  REQUIRE(div_cmp.desired[Axis::Y].dim == Dim::Percent);
  REQUIRE(div_cmp.desired[Axis::X].value == 0.8f);
  REQUIRE(div_cmp.desired[Axis::Y].value == 0.6f);
  al.calculate_standalone(div_cmp);
  REQUIRE(div_cmp.computed[Axis::X] == -1.f);
  REQUIRE(div_cmp.computed[Axis::Y] == -1.f);
}

}}}} // namespaces