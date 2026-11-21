// hover_focus_name_test.cpp
// R-023: hover/focus queries by debug name. UIContext::is_hot/has_focus take
// an entity id, so render and game systems that know a widget's name had to
// re-run their own hit test (cartographer). is_hovered/is_focused/
// is_focus_visible answer by debug name, with the same first-match lookup
// the e2e expect_hovered command uses.
//
// Hover goes through the real hit-target resolution; focus through set_focus.

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

// Two named, clickable divs, one above the other.
struct Two {
  ElementResult a;
  ElementResult b;
};

// Emits the divs without laying out, so multi-frame tests can re-emit after
// begin_frame() the way a real app rebuilds its UI each frame.
Two emit_two(ImmTestHarness &h) {
  auto a = div(h.context(), mk(h.root(), 0),
               ComponentConfig{}
                   .with_size(ComponentSize{pixels(100), pixels(40)})
                   .with_absolute_position(0.f, 0.f)
                   .with_debug_name("desk_hotspot"));
  a.ent().addComponentIfMissing<HasClickListener>([](Entity &) {});
  auto b = div(h.context(), mk(h.root(), 1),
               ComponentConfig{}
                   .with_size(ComponentSize{pixels(100), pixels(40)})
                   .with_absolute_position(0.f, 100.f)
                   .with_debug_name("other"));
  b.ent().addComponentIfMissing<HasClickListener>([](Entity &) {});
  return Two{a, b};
}

Two build_two(ImmTestHarness &h) {
  auto t = emit_two(h);
  h.layout_only();
  return t;
}
} // namespace

// The by-name hover query agrees with the by-id one, element for element.
TEST(hover_query_by_debug_name_matches_is_hot) {
  ImmTestHarness h;
  auto t = build_two(h);

  resolve_at(h, 50.f, 20.f); // over A
  CHECK(h.context().is_hovered("desk_hotspot"));
  CHECK(!h.context().is_hovered("other"));
  CHECK(h.context().is_hot(t.a.ent().id));

  resolve_at(h, 50.f, 120.f); // over B
  CHECK(!h.context().is_hovered("desk_hotspot"));
  CHECK(h.context().is_hovered("other"));
}

// The same answers hold frame after frame, not just on the build frame.
TEST(hover_and_focus_queries_hold_across_frames) {
  ImmTestHarness h;
  auto t = build_two(h);
  h.context().set_focus(t.b.ent().id);
  h.context().visual_focus_id = t.b.ent().id;

  for (int frame = 0; frame < 3; frame++) {
    h.begin_frame();
    emit_two(h);
    h.layout_only();
    resolve_at(h, 50.f, 20.f); // over A every frame
    CHECK(h.context().is_hovered("desk_hotspot"));
    CHECK(h.context().is_focused("other"));
    CHECK(!h.context().is_focused("desk_hotspot"));
    CHECK(h.context().is_focus_visible("other"));
  }
}

// Focus by name follows set_focus, and the ring query follows visual_focus_id.
TEST(focus_query_by_debug_name) {
  ImmTestHarness h;
  auto t = build_two(h);

  h.context().set_focus(t.a.ent().id);
  h.context().visual_focus_id = t.a.ent().id;
  CHECK(h.context().is_focused("desk_hotspot"));
  CHECK(!h.context().is_focused("other"));
  CHECK(h.context().is_focus_visible("desk_hotspot"));
  CHECK(!h.context().is_focus_visible("other"));

  h.context().set_focus(t.b.ent().id);
  h.context().visual_focus_id = t.b.ent().id;
  CHECK(!h.context().is_focused("desk_hotspot"));
  CHECK(h.context().is_focused("other"));
  CHECK(!h.context().is_focus_visible("desk_hotspot"));
  CHECK(h.context().is_focus_visible("other"));
}

// A name no entity carries is false -- including while nothing is hovered,
// where hot_id sits at ROOT and an unguarded lookup would read true.
TEST(missing_debug_name_answers_false) {
  ImmTestHarness h;
  build_two(h);

  resolve_at(h, 400.f, 300.f); // over nothing
  CHECK(h.context().hot_id == h.context().ROOT);
  CHECK(!h.context().is_hovered("nope"));
  CHECK(!h.context().is_focused("nope"));
  CHECK(!h.context().is_focus_visible("nope"));
}

// A name shared by several widgets takes the first match, same as expect_hovered.
TEST(shared_debug_name_takes_the_first_match) {
  ImmTestHarness h;
  auto one = div(h.context(), mk(h.root(), 0),
                 ComponentConfig{}
                     .with_size(ComponentSize{pixels(100), pixels(40)})
                     .with_absolute_position(0.f, 0.f)
                     .with_debug_name("row"));
  one.ent().addComponentIfMissing<HasClickListener>([](Entity &) {});
  auto two = div(h.context(), mk(h.root(), 1),
                 ComponentConfig{}
                     .with_size(ComponentSize{pixels(100), pixels(40)})
                     .with_absolute_position(0.f, 100.f)
                     .with_debug_name("row"));
  two.ent().addComponentIfMissing<HasClickListener>([](Entity &) {});
  h.layout_only();

  resolve_at(h, 50.f, 20.f); // over the first row
  CHECK(h.context().is_hovered("row"));
  CHECK(h.context().is_hot(one.ent().id));

  resolve_at(h, 50.f, 120.f); // over the second row, same name
  CHECK(h.context().is_hot(two.ent().id));
  CHECK(!h.context().is_hovered("row"));
}

int main() { return ui_test::run_registered_tests("hover/focus by debug name"); }
