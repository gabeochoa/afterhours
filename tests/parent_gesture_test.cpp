// parent_gesture_test.cpp -- R-020: a draggable parent and a child button
// must arbitrate one press: tap -> button, drag past threshold -> parent.
#include "ui_test_harness.h"
using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;
using ui_test::TestInputAction;

namespace {
void mark_rendered() {
  for (const auto &e : UICollectionHolder::get().collection.get_entities())
    if (e && e->has<UIComponent>()) e->get<UIComponent>().was_rendered_to_screen = true;
}
struct Scene { ImmTestHarness h; int btn = 0, drag = 0; EntityID btn_id = -1; };
void run_systems(ImmTestHarness &h) {
  ResolveHitTarget<TestInputAction> r; r.context = &h.context(); r.resolve();
  HandleClicks<TestInputAction> c; c.context = &h.context();
  HandleDrags<TestInputAction> d; d.context = &h.context();
  for (const auto &e : UICollectionHolder::get().collection.get_entities()) {
    if (!e || !e->has<UIComponent>()) continue;
    if (e->has<HasClickListener>()) c.for_each_with(*e, e->get<UIComponent>(), e->get<HasClickListener>(), 0.f);
    if (e->has<HasDragListener>()) d.for_each_with(*e, e->get<UIComponent>(), e->get<HasDragListener>(), 0.f);
  }
}
void frame(Scene &s, float x, bool down, bool just, bool rel, bool moved) {
  s.h.begin_frame();
  auto parent = div(s.h.context(), mk(s.h.root(), 0),
      ComponentConfig{}.with_size({pixels(300), pixels(100)}).with_absolute_position(0, 0).with_debug_name("parent"));
  parent.ent().addComponentIfMissing<HasDragListener>([&](Entity &) { s.drag++; });
  auto b = button(s.h.context(), mk(parent.ent(), 0),
      ComponentConfig{}.with_size({pixels(80), pixels(30)}).with_label("B").with_debug_name("btn"));
  b.ent().get<HasClickListener>().cb = [&](Entity &) { s.btn++; };
  s.btn_id = b.ent().id;
  s.h.layout_only(); mark_rendered();
  auto &m = s.h.context().mouse;
  m.pos = {x, 15}; m.left_down = down; m.just_pressed = just; m.just_released = rel; m.press_moved = moved;
  if (just) { m.press_pos = m.pos; s.h.context().set_hot(s.h.context().ROOT); s.h.context().set_active(s.h.context().ROOT); }
  run_systems(s.h);
}
} // namespace

TEST(tap_on_child_button_in_draggable_parent_fires_button_only) {
  Scene s; frame(s, 40, true, true, false, false); frame(s, 40, false, false, true, false);
  CHECK(s.btn == 1); CHECK(s.drag == 0);
}
TEST(drag_starting_on_button_claims_parent_past_threshold_without_firing) {
  Scene s; frame(s, 40, true, true, false, false);
  CHECK(s.btn == 0); // press must be held pending arbitration under a drag parent
  frame(s, 90, true, false, false, true);
  CHECK(s.drag >= 1); CHECK(s.btn == 0);
  frame(s, 90, false, false, true, false);
  CHECK(s.btn == 0);
}
TEST(drag_starting_on_parent_unaffected) {
  Scene s; frame(s, 200, true, true, false, false); frame(s, 240, true, false, false, true);
  CHECK(s.drag >= 1); CHECK(s.btn == 0);
}
TEST(tap_on_child_button_in_scrollable_parent_fires_button) {
  ImmTestHarness h; int btn = 0;
  h.begin_frame();
  auto view = div(h.context(), mk(h.root(), 0),
      ComponentConfig{}.with_size({pixels(200), pixels(80)}).with_absolute_position(0, 0).with_overflow(Overflow::Scroll).with_debug_name("view"));
  auto b = button(h.context(), mk(view.ent(), 0),
      ComponentConfig{}.with_size({pixels(80), pixels(30)}).with_label("B").with_debug_name("sbtn"));
  b.ent().get<HasClickListener>().cb = [&](Entity &) { btn++; };
  h.layout_only(); mark_rendered();
  auto &m = h.context().mouse;
  m.pos = {40, 15}; m.left_down = true; m.just_pressed = true; m.press_moved = false;
  h.context().set_hot(h.context().ROOT); h.context().set_active(h.context().ROOT);
  ResolveHitTarget<TestInputAction> r; r.context = &h.context(); r.resolve();
  HandleClicks<TestInputAction> c; c.context = &h.context();
  for (const auto &e : UICollectionHolder::get().collection.get_entities())
    if (e && e->has<UIComponent>() && e->has<HasClickListener>())
      c.for_each_with(*e, e->get<UIComponent>(), e->get<HasClickListener>(), 0.f);
  CHECK(btn == 1);
}
int main() { return ui_test::run_registered_tests("parent gestures"); }
