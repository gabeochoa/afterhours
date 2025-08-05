#pragma once
#include "../../../../vendor/catch2/catch.hpp"
#include "../../../../src/plugins/autolayout.h"
namespace afterhours { namespace ui { namespace component_tests { namespace percent_tests {

TEST_CASE("CheckboxNoLabelComponentPercentBasic", "[AutoLayout][ComponentSize][CheckboxNoLabel][Percent]") {
  AutoLayout al;
  UIComponent checkbox_no_label_cmp(1);
  ComponentSize checkbox_size = ComponentSize{percent(0.03f), percent(0.03f)};
  checkbox_no_label_cmp.desired[Axis::X] = checkbox_size.x_axis;
  checkbox_no_label_cmp.desired[Axis::Y] = checkbox_size.y_axis;
  REQUIRE(checkbox_no_label_cmp.desired[Axis::X].dim == Dim::Percent);
  REQUIRE(checkbox_no_label_cmp.desired[Axis::Y].dim == Dim::Percent);
  REQUIRE(checkbox_no_label_cmp.desired[Axis::X].value == 0.03f);
  REQUIRE(checkbox_no_label_cmp.desired[Axis::Y].value == 0.03f);
  al.calculate_standalone(checkbox_no_label_cmp);
  REQUIRE(checkbox_no_label_cmp.computed[Axis::X] == -1.f);
  REQUIRE(checkbox_no_label_cmp.computed[Axis::Y] == -1.f);
}

}}}} // namespaces