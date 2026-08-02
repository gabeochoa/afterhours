// size_constraints_test.cpp
// Covers two ComponentConfig ergonomics additions:
//
//   1. with_min_width/with_max_width/with_min_height/with_max_height —
//      UIComponent has always had min_size/max_size and AutoLayout has always
//      applied them (apply_size_constraints), but ComponentConfig never exposed
//      them, so no immediate-mode caller could reach the feature.
//
//   2. ComponentConfig implicit construction from a string, so callers can
//      write button(ctx, mk(e), "Click Me") instead of
//      ComponentConfig{}.with_label("Click Me").
//
// The regression that matters most for (1) is the unconstrained case: the
// wire-up in apply_layout calls set_min_width/set_max_width unconditionally, so
// a default (Dim::None) constraint must pass through without clamping to zero.

#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

namespace {
// 400x100 row to hang constrained children off of.
ElementResult make_row(ImmTestHarness &h) {
  return hstack(h.context(), mk(h.root(), 0),
                ComponentConfig{}
                    .with_size(ComponentSize{pixels(400), pixels(100)})
                    .with_debug_name("row"));
}
} // namespace

// A default-constructed config leaves min/max as Dim::None, which must not
// clamp anything. Without this, wiring set_min_width unconditionally would
// squash every widget in the library to zero.
TEST(unset_constraints_do_not_clamp) {
  ImmTestHarness h;
  auto row = make_row(h);
  div(h.context(), mk(row.ent(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{percent(1.f), percent(1.f)})
          .with_debug_name("child"));
  h.layout_only();

  UIComponent *child = h.find("child");
  CHECK(child != nullptr);
  if (child) {
    CHECK_APPROX(child->rect().width, 400.f);
    CHECK_APPROX(child->rect().height, 100.f);
  }
}

// max_width caps a child that would otherwise fill the row.
TEST(max_width_caps_percent_child) {
  ImmTestHarness h;
  auto row = make_row(h);
  div(h.context(), mk(row.ent(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{percent(1.f), pixels(40)})
          .with_max_width(pixels(150))
          .with_debug_name("child"));
  h.layout_only();

  UIComponent *child = h.find("child");
  CHECK(child != nullptr);
  if (child) {
    CHECK_APPROX(child->rect().width, 150.f); // capped, not 400
    CHECK_APPROX(child->rect().height, 40.f); // other axis untouched
  }
}

// min_width raises a child that asked for less.
TEST(min_width_raises_small_child) {
  ImmTestHarness h;
  auto row = make_row(h);
  div(h.context(), mk(row.ent(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(50), pixels(40)})
          .with_min_width(pixels(120))
          .with_debug_name("child"));
  h.layout_only();

  UIComponent *child = h.find("child");
  CHECK(child != nullptr);
  if (child)
    CHECK_APPROX(child->rect().width, 120.f);
}

// A constraint that is not binding leaves the desired size alone.
TEST(non_binding_constraint_is_noop) {
  ImmTestHarness h;
  auto row = make_row(h);
  div(h.context(), mk(row.ent(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(200), pixels(40)})
          .with_min_width(pixels(100))
          .with_max_width(pixels(300))
          .with_debug_name("child"));
  h.layout_only();

  UIComponent *child = h.find("child");
  CHECK(child != nullptr);
  if (child)
    CHECK_APPROX(child->rect().width, 200.f);
}

// The height axis is wired independently of the width axis.
TEST(max_height_caps_percent_child) {
  ImmTestHarness h;
  auto row = make_row(h);
  div(h.context(), mk(row.ent(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(80), percent(1.f)})
          .with_max_height(pixels(25))
          .with_debug_name("child"));
  h.layout_only();

  UIComponent *child = h.find("child");
  CHECK(child != nullptr);
  if (child) {
    CHECK_APPROX(child->rect().height, 25.f);
    CHECK_APPROX(child->rect().width, 80.f);
  }
}

// Percent constraints resolve against the parent's content box, so a max of
// percent(0.25) on a 400-wide row means 100px.
TEST(percent_constraint_resolves_against_parent) {
  ImmTestHarness h;
  auto row = make_row(h);
  div(h.context(), mk(row.ent(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{percent(1.f), pixels(40)})
          .with_max_width(percent(0.25f))
          .with_debug_name("child"));
  h.layout_only();

  UIComponent *child = h.find("child");
  CHECK(child != nullptr);
  if (child)
    CHECK_APPROX(child->rect().width, 100.f);
}

// A bare string literal converts to a ComponentConfig carrying that label,
// leaving every other field at its default.
TEST(string_literal_converts_to_config) {
  ComponentConfig from_literal = "Click Me";
  CHECK(from_literal.label == "Click Me");
  CHECK(from_literal.size.is_default);

  ComponentConfig from_view = std::string_view{"View"};
  CHECK(from_view.label == "View");
}

// ...and the conversion fires at a real widget call site.
TEST(widgets_accept_bare_string_config) {
  ImmTestHarness h;
  auto row = make_row(h);
  auto b = button(h.context(), mk(row.ent(), 0), "Click Me");
  auto d = div(h.context(), mk(row.ent(), 1), "Hello");
  h.layout_only();

  CHECK(b.ent().has<HasLabel>());
  if (b.ent().has<HasLabel>())
    CHECK(b.ent().get<HasLabel>().label == "Click Me");

  CHECK(d.ent().has<HasLabel>());
  if (d.ent().has<HasLabel>())
    CHECK(d.ent().get<HasLabel>().label == "Hello");
}

int main() { return ui_test::run_registered_tests("size constraints"); }
