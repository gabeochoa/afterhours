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
//
// Then imm::divider and imm::hsplit_pane / imm::vsplit_pane, the resizable
// forms. Those add a drag: the divider reports how far it moved this frame in
// rect() space, and the pane turns that into a new ratio.

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

// Stand in for HandleDrags, which the harness does not run. Marking the
// listener down is exactly what that system does while an element is active,
// and it is the only thing a divider reads on the next build pass.
void hold(const ElementResult &bar) {
  bar.ent().get<HasDragListener>().down = true;
}
void release(const ElementResult &bar) {
  bar.ent().get<HasDragListener>().down = false;
}
} // namespace

// Fixed / expand / fixed — the classic title-bar, body, status-bar skeleton.
TEST(vsplit_fixed_expand_fixed) {
  ImmTestHarness h;
  auto [title, main, status] =
      vsplit(h.context(), mk(h.root(), 0),
             {pixels(30), expand(1.f), pixels(50)});
  h.layout_only();

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
  h.layout_only();

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
  h.layout_only();

  CHECK_APPROX(rect_of(head).height, 100.f);
  CHECK_APPROX(rect_of(small).height, 125.f); // (600-100) * 1/4
  CHECK_APPROX(rect_of(big).height, 375.f);   // (600-100) * 3/4
}

// hsplit is the row-major counterpart: sizes drive width, height spans.
TEST(hsplit_sidebar_and_content) {
  ImmTestHarness h;
  auto [sidebar, content] =
      hsplit(h.context(), mk(h.root(), 0), {pixels(200), expand(1.f)});
  h.layout_only();

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
  h.layout_only();

  CHECK_APPROX(rect_of(left).width, 200.f);
  CHECK_APPROX(rect_of(right).width, 600.f);
}

// A config sizes and positions the container; the split divides what it gets.
TEST(split_respects_container_config) {
  ImmTestHarness h;
  auto [top, bottom] = vsplit(
      h.context(), mk(h.root(), 0), {expand(1.f), expand(1.f)},
      ComponentConfig{}.with_size(ComponentSize{pixels(400), pixels(200)}));
  h.layout_only();

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
  h.layout_only();

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
  h.layout_only();

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
  h.layout_only();

  CHECK(two.size() == 2);
  CHECK(five.size() == 5);
  CHECK_APPROX(rect_of(five[0]).width, 100.f); // 500 / 5
  CHECK_APPROX(rect_of(five[4]).width, 100.f);
}

// ---------------------------------------------------------------------------
// divider / hsplit_pane / vsplit_pane
// ---------------------------------------------------------------------------

// A bare divider is thin across its drag axis, full-length along the other, and
// asks for the matching resize cursor.
TEST(divider_shape_and_cursor) {
  ImmTestHarness h;
  auto row = hstack(h.context(), mk(h.root(), 0),
                    ComponentConfig{}.with_size(
                        ComponentSize{percent(1.f), percent(1.f)}));
  auto vbar = divider(h.context(), mk(row.ent(), 0), Axis::X);
  h.layout_only();

  CHECK_APPROX(rect_of(vbar).width, 4.f);
  CHECK_APPROX(rect_of(vbar).height, 600.f);
  CHECK(vbar.ent().has<HasCursor>());
  CHECK(vbar.ent().get<HasCursor>().cursor == CursorType::ResizeH);
}

TEST(divider_y_axis_is_a_horizontal_bar) {
  ImmTestHarness h;
  auto hbar = divider(h.context(), mk(h.root(), 0), Axis::Y);
  h.layout_only();

  CHECK_APPROX(rect_of(hbar).width, 800.f);
  CHECK_APPROX(rect_of(hbar).height, 4.f);
  CHECK(hbar.ent().get<HasCursor>().cursor == CursorType::ResizeV);
}

// Not being dragged means no movement, however far the pointer travelled.
TEST(divider_reports_nothing_when_not_dragged) {
  ImmTestHarness h;
  h.context().mouse.delta = {50.f, 50.f};
  auto bar = divider(h.context(), mk(h.root(), 0), Axis::X);
  h.layout_only();

  CHECK(!bar);
  CHECK_APPROX(bar.as<float>(), 0.f);
}

// While held it reports the frame's movement along its own axis only — a
// vertical bar ignores vertical mouse travel.
//
// Every frame is emitted from ONE call site on purpose: mk() hashes the source
// location, so the same widget written on two lines is two entities and the
// second frame would silently start from scratch.
TEST(divider_reports_delta_on_its_own_axis) {
  ImmTestHarness h;
  auto frame = [&] { return divider(h.context(), mk(h.root(), 0), Axis::X); };

  hold(frame());
  h.layout_only();

  h.begin_frame();
  h.context().mouse.delta = {12.f, -40.f};
  auto dragged = frame();
  h.layout_only();

  CHECK(dragged);
  CHECK_APPROX(dragged.as<float>(), 12.f);
}

// The ratio drives the first region's share; the divider sits between them and
// the second region soaks up the rest.
TEST(hsplit_pane_initial_geometry) {
  ImmTestHarness h;
  float ratio = 0.25f;
  auto [left, bar, right] = hsplit_pane(h.context(), mk(h.root(), 0), ratio);
  h.layout_only();

  CHECK_APPROX(rect_of(left).width, 200.f);
  CHECK_APPROX(rect_of(bar).width, 4.f);
  CHECK_APPROX(rect_of(right).width, 596.f); // 800 - 200 - 4
  CHECK_APPROX(rect_of(bar).x, 200.f);
  CHECK_APPROX(rect_of(right).x, 204.f);
  CHECK_APPROX(rect_of(left).height, 600.f); // cross axis spans
}

// Dragging converts a pixel delta into a ratio against the container's width,
// and the region resizes the SAME frame rather than a frame later.
TEST(hsplit_pane_drag_updates_ratio_and_width) {
  ImmTestHarness h;
  float ratio = 0.25f;
  auto frame = [&] { return hsplit_pane(h.context(), mk(h.root(), 0), ratio); };

  hold(frame()[1]);
  h.layout_only();

  h.begin_frame();
  h.context().mouse.delta = {80.f, 0.f};
  auto [left, bar, right] = frame();
  h.layout_only();

  CHECK_APPROX(ratio, 0.35f); // 0.25 + 80/800
  CHECK_APPROX(rect_of(left).width, 280.f);
  CHECK_APPROX(rect_of(bar).x, 280.f);
  CHECK_APPROX(rect_of(right).width, 516.f);
}

// Dragging back shrinks it, and releasing freezes the ratio even though the
// pointer keeps moving.
TEST(hsplit_pane_release_stops_tracking) {
  ImmTestHarness h;
  float ratio = 0.5f;
  auto frame = [&] { return hsplit_pane(h.context(), mk(h.root(), 0), ratio); };

  hold(frame()[1]);
  h.layout_only();

  h.begin_frame();
  h.context().mouse.delta = {-120.f, 0.f};
  release(frame()[1]);
  h.layout_only();
  CHECK_APPROX(ratio, 0.35f); // 0.5 - 120/800

  h.begin_frame();
  h.context().mouse.delta = {200.f, 0.f};
  frame();
  h.layout_only();
  CHECK_APPROX(ratio, 0.35f);
}

// A drag past either edge stops at it instead of running the ratio negative or
// past 1, which would make percent() sizing nonsense.
TEST(hsplit_pane_ratio_clamps_to_unit_range) {
  ImmTestHarness h;
  float ratio = 0.9f;
  auto frame = [&] { return hsplit_pane(h.context(), mk(h.root(), 0), ratio); };

  hold(frame()[1]);
  h.layout_only();

  h.begin_frame();
  h.context().mouse.delta = {5000.f, 0.f};
  frame();
  h.layout_only();
  CHECK_APPROX(ratio, 1.f);

  h.begin_frame();
  h.context().mouse.delta = {-5000.f, 0.f};
  frame();
  h.layout_only();
  CHECK_APPROX(ratio, 0.f);
}

// The stacked counterpart splits height and reads vertical movement.
TEST(vsplit_pane_drag_updates_height) {
  ImmTestHarness h;
  float ratio = 0.5f;
  auto frame = [&] { return vsplit_pane(h.context(), mk(h.root(), 0), ratio); };

  {
    auto [t, bar, b] = frame();
    h.layout_only();
    CHECK_APPROX(rect_of(t).height, 300.f);
    CHECK_APPROX(rect_of(bar).height, 4.f);
    CHECK_APPROX(rect_of(b).height, 296.f);
    hold(bar);
  }

  h.begin_frame();
  h.context().mouse.delta = {999.f, 60.f}; // x is ignored on this axis
  auto [top, bar, bottom] = frame();
  h.layout_only();

  CHECK_APPROX(ratio, 0.6f); // 0.5 + 60/600
  CHECK_APPROX(rect_of(top).height, 360.f);
  CHECK_APPROX(rect_of(bottom).y, 364.f);
}

// Panes nest: a vsplit_pane inside an hsplit_pane's expand() region divides
// only that region. The inner regions are percent/expand of a parent that is
// itself expand-sized, which is the arrangement a real sidebar+preview screen
// produces and the one most likely to resolve to zero.
TEST(split_panes_nest_inside_an_expand_region) {
  ImmTestHarness h;
  float outer = 0.25f, inner = 0.6f;
  auto [side, vbar, content] =
      hsplit_pane(h.context(), mk(h.root(), 0), outer);
  auto [top, hbar, bottom] =
      vsplit_pane(h.context(), mk(content.ent(), 0), inner);
  h.layout_only();

  CHECK_APPROX(rect_of(content).width, 596.f); // 800 - 200 - 4
  CHECK_APPROX(rect_of(top).width, 596.f);     // inner spans the region
  CHECK_APPROX(rect_of(top).height, 360.f);    // 600 * 0.6
  CHECK_APPROX(rect_of(hbar).height, 4.f);
  CHECK_APPROX(rect_of(bottom).height, 236.f); // 600 - 360 - 4
  CHECK_APPROX(rect_of(bottom).y, 364.f);
}

// Same nesting, but the outer pane is a fixed box placed absolutely — how a
// lab screen or a floating panel lays one out.
TEST(split_panes_nest_inside_an_absolute_box) {
  ImmTestHarness h;
  float outer = 0.25f, inner = 0.5f;
  auto [side, vbar, content] = hsplit_pane(
      h.context(), mk(h.root(), 0), outer,
      ComponentConfig{}
          .with_size(ComponentSize{pixels(400), pixels(300)})
          .with_absolute_position(20.f, 10.f));
  auto [top, hbar, bottom] =
      vsplit_pane(h.context(), mk(content.ent(), 0), inner);
  h.layout_only();

  CHECK_APPROX(rect_of(side).width, 100.f);
  CHECK_APPROX(rect_of(content).width, 296.f);
  CHECK_APPROX(rect_of(top).height, 150.f);
  CHECK_APPROX(rect_of(bottom).height, 146.f);
  CHECK_APPROX(rect_of(top).x, 124.f); // 20 + 100 + 4
  CHECK_APPROX(rect_of(bottom).y, 164.f);
}

// The regions are ordinary containers, and the pane divides only the box its
// config gives it — same contract as vsplit/hsplit.
TEST(split_pane_respects_config_and_takes_children) {
  ImmTestHarness h;
  float ratio = 0.5f;
  auto [left, bar, right] = hsplit_pane(
      h.context(), mk(h.root(), 0), ratio,
      ComponentConfig{}.with_size(ComponentSize{pixels(404), pixels(200)}));
  auto b = button(h.context(), mk(left.ent(), 0), "Click Me");
  h.layout_only();

  CHECK_APPROX(rect_of(left).width, 202.f); // 404 * 0.5
  CHECK_APPROX(rect_of(right).width, 198.f);
  CHECK_APPROX(rect_of(left).height, 200.f);
  CHECK(b.cmp().parent == left.ent().id);
}

int main() {
  return ui_test::run_registered_tests("vsplit / hsplit / split panes");
}
