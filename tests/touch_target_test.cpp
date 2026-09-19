#include "ui_test_harness.h"
#include <afterhours/src/plugins/ui/validation_systems.h>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

static ValidationConfig &config() {
  return UIStylingDefaults::get().get_validation_config_mut();
}

static void validate(ui_test::ImmTestHarness &h) {
  h.coll.merge_entity_arrays();
  SystemManager systems;
  validation::register_update_systems(systems);
  systems.tick(EntityHelper::get_entities_for_mod(), 0.f);
}

static ComponentConfig target(float width, float height) {
  return ComponentConfig{}.with_label("A").with_size({pixels(width), pixels(height)})
      .with_absolute_position(100, 100).with_skip_grid_snap();
}

TEST(flag_threshold_and_marker_cleanup) {
  ui_test::ImmTestHarness h;
  h.begin_frame();
  auto small = button(h.context(), mk(h.root(), 0), target(32, 44));
  auto exact = button(h.context(), mk(h.root(), 1), target(44, 44));
  auto short_one = button(h.context(), mk(h.root(), 2), target(80, 32));
  auto decoration = div(h.context(), mk(h.root(), 3), target(16, 16));
  h.layout_only();
  config() = {};
  config().mode = ValidationMode::Warn;
  config().highlight_violations = true;
  validate(h);
  CHECK(!small.ent().has<ValidationViolation>());
  config().enforce_min_touch_target = true;
  validate(h);
  CHECK(small.ent().has<ValidationViolation>());
  CHECK(short_one.ent().has<ValidationViolation>());
  CHECK(!exact.ent().has<ValidationViolation>());
  CHECK(!decoration.ent().has<ValidationViolation>());
  small.cmp().set_desired_width(pixels(48));
  h.layout_only();
  validate(h);
  CHECK(!small.ent().has<ValidationViolation>());
  config().min_touch_target_size = 32;
  validate(h);
  CHECK(!small.ent().has<ValidationViolation>());
  CHECK(!short_one.ent().has<ValidationViolation>());
  config().min_touch_target_size = 48;
  validate(h);
  CHECK(exact.ent().has<ValidationViolation>());
  config().enforce_min_touch_target = false;
  validate(h);
  CHECK(!exact.ent().has<ValidationViolation>());
  config().enforce_min_touch_target = true;
  config().mode = ValidationMode::Silent;
  validate(h);
  CHECK(!small.ent().has<ValidationViolation>());
  config() = {};
}

TEST(hidden_disabled_and_pointer_ignored_controls_are_skipped) {
  ui_test::ImmTestHarness h;
  h.begin_frame();
  std::vector<Entity *> controls;
  for (int i = 0; i < 6; ++i)
    controls.push_back(&button(h.context(), mk(h.root(), i), target(20, 20)).ent());
  h.layout_only();
  controls[0]->get<UIComponent>().should_hide = true;
  controls[1]->addComponent<ShouldHide>();
  controls[2]->get<UIComponent>().was_rendered_to_screen = false;
  controls[3]->get<HasLabel>().is_disabled = true;
  controls[4]->addComponent<IgnorePointerEvents>();
  controls[5]->cleanup = true;
  config() = {};
  config().mode = ValidationMode::Warn;
  config().enforce_min_touch_target = true;
  config().highlight_violations = true;
  validate(h);
  for (auto *e : controls) CHECK(!e->has<ValidationViolation>());
  controls[0]->get<UIComponent>().should_hide = false;
  h.root().addComponent<ShouldHide>();
  validate(h);
  CHECK(!controls[0]->has<ValidationViolation>());
  h.root().removeComponent<ShouldHide>();
  h.root().addComponent<HasLabel>("", true);
  validate(h);
  CHECK(!controls[0]->has<ValidationViolation>());
  config() = {};
}

TEST(hit_bounds_include_scroll_clipping_transform_and_window_edges) {
  ui_test::ImmTestHarness h;
  h.begin_frame();
  auto view = div(h.context(), mk(h.root(), 0), target(100, 100)
      .with_overflow(Overflow::Scroll, Axis::Y));
  auto clipped = button(h.context(), mk(view.ent(), 0), target(60, 60)
      .with_absolute_position(0, 0));
  auto offscreen = button(h.context(), mk(h.root(), 1), target(20, 20)
      .with_absolute_position(900, 100));
  auto edge = button(h.context(), mk(h.root(), 2), target(60, 60)
      .with_absolute_position(780, 100));
  auto scaled = button(h.context(), mk(h.root(), 3), target(60, 60).with_scale(.5f));
  auto drag = div(h.context(), mk(h.root(), 4), target(20, 60));
  drag.ent().addComponent<HasDragListener>([](Entity &) {});
  h.layout_only();
  auto &scroll = view.ent().get<HasScrollView>();
  scroll.viewport_size = {100, 100};
  scroll.content_size = {100, 200};
  scroll.scroll_offset.y = 30;
  config() = {};
  config().mode = ValidationMode::Warn;
  config().enforce_min_touch_target = true;
  config().highlight_violations = true;
  validate(h);
  CHECK(clipped.ent().has<ValidationViolation>());
  CHECK(edge.ent().has<ValidationViolation>());
  CHECK(scaled.ent().has<ValidationViolation>());
  CHECK(drag.ent().has<ValidationViolation>());
  CHECK(!offscreen.ent().has<ValidationViolation>());
  scroll.scroll_offset.y = 80;
  validate(h);
  CHECK(!clipped.ent().has<ValidationViolation>());
  config() = {};
}

TEST(overlay_and_cleanup_visit_a_shared_root_once) {
  ui_test::ImmTestHarness h;
  h.begin_frame();
  auto small = button(h.context(), mk(h.root(), 0), target(20, 20));
  h.layout_only();
#ifndef AFTER_HOURS_UI_SINGLE_COLLECTION
  auto &defaults = EntityHelper::get_default_collection();
  for (const auto &e : h.coll.get_entities())
    if (e.get() == &h.root()) defaults.temp_entities.push_back(e);
  defaults.merge_entity_arrays();
#endif
  config() = {};
  config().mode = ValidationMode::Warn;
  config().enforce_min_touch_target = true;
  config().highlight_violations = true;
  validate(h);
  CHECK(small.ent().has<ValidationViolation>());
  h.root().addComponent<ValidationViolation>("root", "test", 1.f);
  clear_draw_calls();
  validation::RenderOverlay<ui_test::TestInputAction> overlay;
  overlay.for_each_with(h.context_entity(), h.context(), 0);
  CHECK(draw_calls().size() == 10);
  config().highlight_violations = false;
  validate(h);
  CHECK(!small.ent().has<ValidationViolation>());
  CHECK(!h.root().has<ValidationViolation>());
  clear_draw_calls();
  overlay.for_each_with(h.context_entity(), h.context(), 0);
  CHECK(draw_calls().empty());
  config() = {};
}

int main() { return ui_test::run_registered_tests("touch target validation"); }
