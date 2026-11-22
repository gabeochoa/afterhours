#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

namespace {

void resolve_at(ImmTestHarness &h, float x, float y) {
  h.context().mouse.pos = Vector2Type{x, y};
  h.context().mouse.left_down = false;
  ResolveHitTarget<ui_test::TestInputAction> sys;
  sys.context = &h.context();
  sys.resolve();
}

struct Two {
  ElementResult a;
  ElementResult b;
};

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
}

TEST(hover_query_by_debug_name_matches_is_hot) {
  ImmTestHarness h;
  auto t = build_two(h);

  resolve_at(h, 50.f, 20.f);
  CHECK(h.context().is_hovered("desk_hotspot"));
  CHECK(!h.context().is_hovered("other"));
  CHECK(h.context().is_hot(t.a.ent().id));

  resolve_at(h, 50.f, 120.f);
  CHECK(!h.context().is_hovered("desk_hotspot"));
  CHECK(h.context().is_hovered("other"));
}

TEST(hover_and_focus_queries_hold_across_frames) {
  ImmTestHarness h;
  auto t = build_two(h);
  h.context().set_focus(t.b.ent().id);
  h.context().visual_focus_id = t.b.ent().id;

  for (int frame = 0; frame < 3; frame++) {
    h.begin_frame();
    emit_two(h);
    h.layout_only();
    resolve_at(h, 50.f, 20.f);
    CHECK(h.context().is_hovered("desk_hotspot"));
    CHECK(h.context().is_focused("other"));
    CHECK(!h.context().is_focused("desk_hotspot"));
    CHECK(h.context().is_focus_visible("other"));
  }
}

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

TEST(missing_debug_name_answers_false) {
  ImmTestHarness h;
  build_two(h);

  resolve_at(h, 400.f, 300.f);
  CHECK(h.context().hot_id == h.context().ROOT);
  CHECK(!h.context().is_hovered("nope"));
  CHECK(!h.context().is_focused("nope"));
  CHECK(!h.context().is_focus_visible("nope"));
}

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

  resolve_at(h, 50.f, 20.f);
  CHECK(h.context().is_hovered("row"));
  CHECK(h.context().is_hot(one.ent().id));

  resolve_at(h, 50.f, 120.f);
  CHECK(h.context().is_hot(two.ent().id));
  CHECK(!h.context().is_hovered("row"));
}

int main() { return ui_test::run_registered_tests("hover/focus by debug name"); }
