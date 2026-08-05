// subtree_hover_test.cpp
// D25: asking "is the mouse anywhere inside this element?", and opting an
// element out of hit-testing entirely.
//
// There is one global hot_id, so a hoverable child steals its parent's hover
// state. A row with a trailing toggle loses its hover fill the instant the
// pointer crosses onto the toggle -- which is a visible flicker, and the
// reason an app ended up caching child entity ids in a static map just to OR
// their hot state back into the row.
//
// These drive ResolveHitTarget for real rather than calling the query on a
// hand-set hot_id, so they cover the part that actually decides hotness.

#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

namespace {

// Runs the real hit-target resolution with the mouse at (x, y).
void resolve_at(ImmTestHarness &h, float x, float y) {
  h.context().mouse.pos = Vector2Type{x, y};
  h.context().mouse.left_down = false;
  ResolveHitTarget<ui_test::TestInputAction> sys;
  sys.context = &h.context();
  sys.resolve();
}

// A row with a clickable child in its right-hand end, both hit-testable.
struct Row {
  ElementResult row;
  ElementResult child;
};

Row build_row(ImmTestHarness &h, bool child_ignores) {
  auto row = div(h.context(), mk(h.root(), 0),
                 ComponentConfig{}
                     .with_size(ComponentSize{pixels(300), pixels(40)})
                     .with_absolute_position(0.f, 0.f)
                     .with_debug_name("row"));
  // A click listener is what makes something a hit candidate at all.
  row.ent().addComponentIfMissing<HasClickListener>([](Entity &) {});

  auto child = div(h.context(), mk(row.ent(), 0),
                   ComponentConfig{}
                       .with_size(ComponentSize{pixels(40), pixels(40)})
                       .with_absolute_position(260.f, 0.f)
                       .with_ignore_pointer_events(child_ignores)
                       .with_debug_name("star"));
  child.ent().addComponentIfMissing<HasClickListener>([](Entity &) {});

  h.layout_only();
  return Row{row, child};
}
} // namespace

// The bug, stated as a test: hovering the child makes the child hot, so the
// row's own is_hot() goes false and its fill drops out.
TEST(a_hoverable_child_takes_hot_away_from_its_row) {
  ImmTestHarness h;
  auto r = build_row(h, /*child_ignores=*/false);

  resolve_at(h, 20.f, 20.f); // over the row, away from the child
  CHECK(h.context().is_hot(r.row.ent().id));

  resolve_at(h, 280.f, 20.f); // over the child
  CHECK(h.context().is_hot(r.child.ent().id));
  CHECK(!h.context().is_hot(r.row.ent().id));
}

// ...and the fix: the row asks about its subtree instead and stays lit for
// both positions.
TEST(mouse_in_subtree_covers_the_row_and_its_child) {
  ImmTestHarness h;
  auto r = build_row(h, /*child_ignores=*/false);
  const EntityID row_id = r.row.ent().id;

  resolve_at(h, 20.f, 20.f);
  CHECK(h.context().mouse_in_subtree(row_id));

  resolve_at(h, 280.f, 20.f);
  CHECK(h.context().mouse_in_subtree(row_id));

  // Off the row entirely: not hot, and not in the subtree either.
  resolve_at(h, 20.f, 300.f);
  CHECK(!h.context().mouse_in_subtree(row_id));
}

// The query must not report true for an unrelated element that merely happens
// to be hot -- otherwise every row lights up at once.
TEST(mouse_in_subtree_is_false_for_an_unrelated_element) {
  ImmTestHarness h;
  auto a = div(h.context(), mk(h.root(), 0),
               ComponentConfig{}
                   .with_size(ComponentSize{pixels(100), pixels(40)})
                   .with_absolute_position(0.f, 0.f)
                   .with_debug_name("row_a"));
  a.ent().addComponentIfMissing<HasClickListener>([](Entity &) {});
  auto b = div(h.context(), mk(h.root(), 1),
               ComponentConfig{}
                   .with_size(ComponentSize{pixels(100), pixels(40)})
                   .with_absolute_position(0.f, 100.f)
                   .with_debug_name("row_b"));
  b.ent().addComponentIfMissing<HasClickListener>([](Entity &) {});
  h.layout_only();

  resolve_at(h, 50.f, 20.f); // over A
  CHECK(h.context().mouse_in_subtree(a.ent().id));
  CHECK(!h.context().mouse_in_subtree(b.ent().id));
}

// An element itself, with no children, still answers for its own hover.
TEST(mouse_in_subtree_includes_the_element_itself) {
  ImmTestHarness h;
  auto r = build_row(h, false);
  resolve_at(h, 280.f, 20.f);
  CHECK(h.context().mouse_in_subtree(r.child.ent().id));
}

// ---------------------------------------------------------------------------
// Ignoring pointer events
// ---------------------------------------------------------------------------

// With the flag set, the child never wins the hit even though the pointer
// is over it and it has a click listener -- the row behind it does.
TEST(ignore_pointer_events_hands_the_hit_to_the_element_behind) {
  ImmTestHarness h;
  auto r = build_row(h, /*child_ignores=*/true);

  resolve_at(h, 280.f, 20.f); // directly over the child
  CHECK(!h.context().is_hot(r.child.ent().id));
  CHECK(h.context().is_hot(r.row.ent().id));
}

// Without it, the same geometry gives the child the hit. Pins that the test
// above is measuring the flag and not the layout.
TEST(without_ignore_pointer_events_the_child_wins_the_same_hit) {
  ImmTestHarness h;
  auto r = build_row(h, /*child_ignores=*/false);

  resolve_at(h, 280.f, 20.f);
  CHECK(h.context().is_hot(r.child.ent().id));
}

// Ignoring pointer events is about the mouse only. It must not be confused with
// with_skip_tabbing, which is focus-order only and leaves hit-testing alone.
TEST(skip_tabbing_does_not_affect_hit_testing) {
  ImmTestHarness h;
  auto row = div(h.context(), mk(h.root(), 0),
                 ComponentConfig{}
                     .with_size(ComponentSize{pixels(300), pixels(40)})
                     .with_absolute_position(0.f, 0.f)
                     .with_debug_name("row"));
  row.ent().addComponentIfMissing<HasClickListener>([](Entity &) {});
  auto child = div(h.context(), mk(row.ent(), 0),
                   ComponentConfig{}
                       .with_size(ComponentSize{pixels(40), pixels(40)})
                       .with_absolute_position(260.f, 0.f)
                       .with_skip_tabbing(true)
                       .with_debug_name("star"));
  child.ent().addComponentIfMissing<HasClickListener>([](Entity &) {});
  h.layout_only();

  resolve_at(h, 280.f, 20.f);
  CHECK(h.context().is_hot(child.ent().id));
}

int main() { return ui_test::run_registered_tests("subtree hover"); }
