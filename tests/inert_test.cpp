#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

TEST(an_inert_panel_takes_no_clicks_or_focus_anywhere_inside_it) {
  ImmTestHarness h;
  EntityID button_id = -1;
  auto build = [&](bool fading) {
    h.begin_frame();
    auto panel = div(h.context(), mk(h.root(), 0),
                     ComponentConfig{}
                         .with_size({pixels(300), pixels(100)})
                         .with_absolute_position(0, 0)
                         .with_inert(fading));
    auto row = div(h.context(), mk(panel.ent(), 0),
                   ComponentConfig{}.with_size({pixels(300), pixels(40)}));
    auto btn = button(h.context(), mk(row.ent(), 0),
                      ComponentConfig{}.with_size({pixels(100), pixels(40)}).with_label("Resume"));
    button_id = btn.id();
    h.layout_only();
    ResolveHitTarget<ui_test::TestInputAction> resolver;
    resolver.context = &h.context();
    h.context().mouse.pos = {50, 20};
    resolver.resolve();
  };

  build(true);
  Entity &btn = UICollectionHolder::getEntityForIDEnforce(button_id);
  CHECK(!h.context().is_hot(button_id));
  CHECK(!can_be_focused(h.context(), btn));

  build(false);
  CHECK(h.context().is_hot(button_id));
  CHECK(can_be_focused(h.context(), btn));
}

int main() { return ui_test::run_registered_tests("inert"); }
