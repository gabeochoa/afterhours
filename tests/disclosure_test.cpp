// disclosure_test.cpp
// Tests for the imm::disclosure widget: collapsed hides the body, expanded
// shows it, and the header toggles by mouse click and by keyboard
// (WidgetPress on the focused header), with state persisting across frames.

#include "ui_test_harness.h"
#include <afterhours/src/plugins/ui/disclosure.h>

#include <string>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

namespace {

void run_clicks(ImmTestHarness &h) {
  ResolveHitTarget<ui_test::TestInputAction> resolver;
  resolver.context = &h.context();
  resolver.resolve();
  HandleClicks<ui_test::TestInputAction> sys;
  sys.context = &h.context();
  for (const auto &e : UICollectionHolder::get().collection.get_entities()) {
    if (!e || !e->has<UIComponent>() || !e->has<HasClickListener>())
      continue;
    sys.for_each_with(*e, e->get<UIComponent>(), e->get<HasClickListener>(),
                      0.f);
  }
}

Entity &entity_for(UIComponent *cmp) {
  return UICollectionHolder::getEntityForID(cmp->id).asE();
}

struct Fixture {
  ImmTestHarness h;
  int body_builds = 0;
  bool last_changed = false;
  Entity *widget = nullptr;

  ElementResult build(DisclosureConfig cfg = {}) {
    auto result = disclosure(
        h.context(), mk(h.root(), 0), "Advanced",
        [&](Entity &body_entity) {
          body_builds++;
          div(h.context(), mk(body_entity, 0),
              ComponentConfig{}
                  .with_size(ComponentSize{pixels(200), pixels(20)})
                  .with_label("Body text")
                  .with_debug_name("disclosure_body_content"));
        },
        cfg);
    last_changed = static_cast<bool>(result);
    widget = &result.ent();
    h.layout_only();
    return result;
  }

  Entity &header() { return entity_for(h.find("disclosure_header")); }
  bool expanded() { return widget->get<HasDisclosureState>().expanded; }
};

} // namespace

TEST(collapsed_hides_body_expanded_shows) {
  Fixture f;
  f.build();
  CHECK(!f.expanded());
  CHECK(f.body_builds == 0);
  CHECK(f.h.find("disclosure_body") == nullptr);
  CHECK(f.h.find("disclosure_chevron") != nullptr);

  f.h.begin_frame();
  auto result = f.build(DisclosureConfig{.default_expanded = true});
  // default_expanded only seeds new state; existing state stays collapsed.
  CHECK(!f.expanded());
  (void)result;

  Fixture g;
  g.build(DisclosureConfig{.default_expanded = true});
  CHECK(g.expanded());
  CHECK(g.body_builds == 1);
  CHECK(g.h.find("disclosure_body") != nullptr);
  CHECK(g.h.find("disclosure_body_content") != nullptr);
}

TEST(toggle_by_click) {
  Fixture f;
  f.build();
  auto rect = f.header().get<UIComponent>().rect();
  f.h.context().mouse.pos =
      input::MousePosition{rect.x + rect.width / 2.f, rect.y + rect.height / 2.f};
  f.h.context().mouse.left_down = true;
  f.h.context().mouse.just_pressed = true;
  f.h.context().mouse.press_moved = false;
  f.h.context().set_hot(f.h.context().ROOT);
  f.h.context().set_active(f.h.context().ROOT);
  run_clicks(f.h);
  f.h.context().mouse.left_down = false;
  f.h.context().mouse.just_pressed = false;

  f.h.begin_frame();
  f.build();
  CHECK(f.last_changed);
  CHECK(f.expanded());
  CHECK(f.h.find("disclosure_body_content") != nullptr);
}

TEST(toggle_by_keyboard_and_state_persists) {
  Fixture f;
  f.build();
  f.h.context().set_focus(f.header().id);
  f.h.context().last_action = ui_test::TestInputAction::WidgetPress;
  HandleClicks<ui_test::TestInputAction> sys;
  sys.context = &f.h.context();
  auto &header = f.header();
  sys.for_each_with(header, header.get<UIComponent>(),
                    header.get<HasClickListener>(), 0.f);
  CHECK(header.get<HasClickListener>().down);

  for (int frame = 0; frame < 3; frame++) {
    f.h.begin_frame();
    f.build();
    // HandleClicks clears `down` every frame in a real app; do the same so
    // the one activation does not re-toggle on the next build.
    f.header().get<HasClickListener>().down = false;
    CHECK(f.expanded());
    CHECK(f.h.find("disclosure_body_content") != nullptr);
  }
  CHECK(f.body_builds == 3);
}

int main() { return ui_test::run_registered_tests("disclosure_test"); }
