// split_test.cpp
// Geometry tests for imm::vsplit / imm::hsplit — divide a region into N parts
// and get all N back at once, so a screen skeleton is one statement instead of
// N nested divs.
//
// The properties that matter:
//   - regions come back in declared order (pack expansion order)
//   - fixed sizes are honored and expand() soaks up exactly the remainder
//   - regions tile the container with no gaps or overlap
//   - the cross axis spans the container
//   - N is deduced from the size list, and nesting composes

#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

// The harness root is 800x600.
namespace {
Rectangle rect_of(const ElementResult &e) {
  return e.ent().get<UIComponent>().rect();
}
} // namespace

// Fixed / expand / fixed — the classic title-bar, body, status-bar skeleton.
TEST(vsplit_fixed_expand_fixed) {
  ImmTestHarness h;
  auto [title, main, status] =
      vsplit(h.context(), mk(h.root(), 0),
             {pixels(30), expand(1.f), pixels(50)});
  h.layout_and_render();

  CHECK_APPROX(rect_of(title).height, 30.f);
  CHECK_APPROX(rect_of(main).height, 520.f); // 600 - 30 - 50
  CHECK_APPROX(rect_of(status).height, 50.f);

  // Each region spans the container width.
  CHECK_APPROX(rect_of(title).width, 800.f);
  CHECK_APPROX(rect_of(main).width, 800.f);
  CHECK_APPROX(rect_of(status).width, 800.f);
}

// Regions are returned top-to-bottom in the order the sizes were written, and
// they tile the container edge to edge.
TEST(vsplit_regions_tile_in_declared_order) {
  ImmTestHarness h;
  auto [a, b, c] = vsplit(h.context(), mk(h.root(), 0),
                          {pixels(100), pixels(200), expand(1.f)});
  h.layout_and_render();

  Rectangle ra = rect_of(a), rb = rect_of(b), rc = rect_of(c);
  CHECK_APPROX(ra.y, 0.f);
  CHECK_APPROX(rb.y, ra.y + ra.height); // no gap, no overlap
  CHECK_APPROX(rc.y, rb.y + rb.height);
  CHECK_APPROX(rc.y + rc.height, 600.f); // exactly fills the container
}

// Two expand() regions share the remainder by weight.
TEST(vsplit_expand_weights_share_remainder) {
  ImmTestHarness h;
  auto [head, small, big] = vsplit(h.context(), mk(h.root(), 0),
                                   {pixels(100), expand(1.f), expand(3.f)});
  h.layout_and_render();

  CHECK_APPROX(rect_of(head).height, 100.f);
  CHECK_APPROX(rect_of(small).height, 125.f); // (600-100) * 1/4
  CHECK_APPROX(rect_of(big).height, 375.f);   // (600-100) * 3/4
}

// hsplit is the row-major counterpart: sizes drive width, height spans.
TEST(hsplit_sidebar_and_content) {
  ImmTestHarness h;
  auto [sidebar, content] =
      hsplit(h.context(), mk(h.root(), 0), {pixels(200), expand(1.f)});
  h.layout_and_render();

  CHECK_APPROX(rect_of(sidebar).width, 200.f);
  CHECK_APPROX(rect_of(content).width, 600.f);
  CHECK_APPROX(rect_of(sidebar).height, 600.f); // cross axis spans
  CHECK_APPROX(rect_of(content).height, 600.f);
  CHECK_APPROX(rect_of(content).x, 200.f); // content starts after sidebar
}

// percent() sizes resolve against the container, not the screen.
TEST(hsplit_percent_sizes) {
  ImmTestHarness h;
  auto [left, right] = hsplit(h.context(), mk(h.root(), 0),
                              {percent(0.25f), percent(0.75f)});
  h.layout_and_render();

  CHECK_APPROX(rect_of(left).width, 200.f);
  CHECK_APPROX(rect_of(right).width, 600.f);
}

// A config sizes and positions the container; the split divides what it gets.
TEST(split_respects_container_config) {
  ImmTestHarness h;
  auto [top, bottom] = vsplit(
      h.context(), mk(h.root(), 0), {expand(1.f), expand(1.f)},
      ComponentConfig{}.with_size(ComponentSize{pixels(400), pixels(200)}));
  h.layout_and_render();

  CHECK_APPROX(rect_of(top).width, 400.f);
  CHECK_APPROX(rect_of(top).height, 100.f);
  CHECK_APPROX(rect_of(bottom).height, 100.f);
}

// Splits nest: an hsplit inside a vsplit region divides only that region.
TEST(splits_nest) {
  ImmTestHarness h;
  auto [header, body] =
      vsplit(h.context(), mk(h.root(), 0), {pixels(100), expand(1.f)});
  auto [nav, content] =
      hsplit(h.context(), mk(body.ent(), 0), {pixels(150), expand(1.f)});
  h.layout_and_render();

  CHECK_APPROX(rect_of(header).height, 100.f);
  CHECK_APPROX(rect_of(nav).width, 150.f);
  CHECK_APPROX(rect_of(nav).height, 500.f); // body's height, not the root's
  CHECK_APPROX(rect_of(content).width, 650.f);
  CHECK_APPROX(rect_of(nav).y, 100.f); // sits below the header
}

// Regions are ordinary containers — widgets parent into them normally.
TEST(split_regions_accept_children) {
  ImmTestHarness h;
  auto [left, right] =
      hsplit(h.context(), mk(h.root(), 0), {expand(1.f), expand(1.f)});
  auto b = button(h.context(), mk(left.ent(), 0), "Click Me");
  div(h.context(), mk(right.ent(), 0), "Label");
  h.layout_and_render();

  // The button landed inside the left region, not at the root.
  CHECK(b.cmp().parent == left.ent().id);
  CHECK(rect_of(b).x < 400.f);
}

// Deduction handles a two-region and a five-region split from the same code.
TEST(split_count_is_deduced) {
  ImmTestHarness h;
  auto two = vsplit(h.context(), mk(h.root(), 0), {expand(1.f), expand(1.f)});
  auto five = hsplit(h.context(), mk(h.root(), 1),
                     {expand(1.f), expand(1.f), expand(1.f), expand(1.f),
                      expand(1.f)},
                     ComponentConfig{}.with_size(
                         ComponentSize{pixels(500), pixels(100)}));
  h.layout_and_render();

  CHECK(two.size() == 2);
  CHECK(five.size() == 5);
  CHECK_APPROX(rect_of(five[0]).width, 100.f); // 500 / 5
  CHECK_APPROX(rect_of(five[4]).width, 100.f);
}

int main() { return ui_test::run_registered_tests("vsplit / hsplit"); }
