// R-012 / hanabi #601: an external/native surface owning the pointer must
// suppress UI hover/clicks underneath; releasing ownership restores them.
#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

namespace {
void mark_rendered() {
  for (const auto &e : UICollectionHolder::get().collection.get_entities())
    if (e && e->has<UIComponent>())
      e->get<UIComponent>().was_rendered_to_screen = true;
}
void run_clicks(ImmTestHarness &h) {
  ResolveHitTarget<ui_test::TestInputAction> resolver;
  resolver.context = &h.context();
  resolver.resolve();
  HandleClicks<ui_test::TestInputAction> sys;
  sys.context = &h.context();
  for (const auto &e : UICollectionHolder::get().collection.get_entities()) {
    if (!e || !e->has<UIComponent>() || !e->has<HasClickListener>()) continue;
    sys.for_each_with(*e, e->get<UIComponent>(), e->get<HasClickListener>(), 0.f);
  }
}
} // namespace

TEST(owned_pointer_suppresses_hover_and_click_then_release_restores) {
  ImmTestHarness h;
  int fired = 0;
  EntityID btn_id = -100;
  auto build = [&] {
    h.begin_frame();
    auto b = button(h.context(), mk(h.root(), 0),
                    ComponentConfig{}.with_size({pixels(100), pixels(30)})
                        .with_absolute_position(0.f, 0.f).with_label("B"));
    b.ent().get<HasClickListener>().cb = [&](Entity &) { ++fired; };
    btn_id = b.ent().id;
    h.layout_only();
    mark_rendered();
  };
  auto press = [&] {
    h.context().mouse.pos = {50.f, 15.f};
    h.context().mouse.left_down = true;
    h.context().mouse.just_pressed = true;
    h.context().mouse.just_released = false;
    h.context().mouse.press_moved = false;
    h.context().set_hot(h.context().ROOT);
    h.context().set_active(h.context().ROOT);
    run_clicks(h);
  };

  // Unowned baseline: no behavior change.
  build(); press();
  CHECK(fired == 1);
  CHECK(h.context().is_hot(btn_id));

  // Owned: hover and press both suppressed.
  build();
  h.context().claim_pointer_ownership();
  CHECK(h.context().pointer_is_owned_externally());
  CHECK(h.context().pointer_owned_externally);
  press();
  CHECK(fired == 1);
  CHECK(!h.context().is_hot(btn_id));
  CHECK(h.context().hot_id == h.context().ROOT);
  CHECK(h.context().active_id == h.context().ROOT);
  // Right/middle queries also stay silent while owned.
  h.context().mouse.right_just_released = true;
  CHECK(!h.context().is_right_click(btn_id));
  h.context().mouse.right_just_released = false;

  // Claim mid-press clears an already-claimed active/hot.
  h.context().release_pointer_ownership();
  build(); press();
  CHECK(fired == 2);
  h.context().claim_pointer_ownership();
  CHECK(h.context().active_id == h.context().ROOT);
  CHECK(h.context().hot_id == h.context().ROOT);

  // Release restores normal resolution.
  h.context().release_pointer_ownership();
  CHECK(!h.context().pointer_is_owned_externally());
  build(); press();
  CHECK(fired == 3);
  CHECK(h.context().is_hot(btn_id));
}

int main() { return ui_test::run_registered_tests("pointer owner"); }
