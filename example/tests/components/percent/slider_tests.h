#pragma once
#include "../../../../vendor/catch2/catch.hpp"
#include "../../../../src/plugins/autolayout.h"
namespace afterhours { namespace ui { namespace component_tests { namespace percent_tests {

TEST_CASE("SliderComponentPercentBasic", "[AutoLayout][ComponentSize][Slider][Percent]") {
  AutoLayout al;
  UIComponent slider_cmp(1);
  ComponentSize slider_size = ComponentSize{percent(0.8f), percent(0.05f)};
  slider_cmp.desired[Axis::X] = slider_size.x_axis;
  slider_cmp.desired[Axis::Y] = slider_size.y_axis;
  REQUIRE(slider_cmp.desired[Axis::X].dim == Dim::Percent);
  REQUIRE(slider_cmp.desired[Axis::Y].dim == Dim::Percent);
  REQUIRE(slider_cmp.desired[Axis::X].value == 0.8f);
  REQUIRE(slider_cmp.desired[Axis::Y].value == 0.05f);
  al.calculate_standalone(slider_cmp);
  REQUIRE(slider_cmp.computed[Axis::X] == -1.f);
  REQUIRE(slider_cmp.computed[Axis::Y] == -1.f);
}

}}}} // namespaces