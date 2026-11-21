#include "ui_test_harness.h"

#include <afterhours/src/plugins/ui_motion.h>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

static bool alive(ImmTestHarness &h, EntityID id) {
  (void)h;
  return UICollectionHolder::getEntityForID(id).valid();
}

static Entity &ent(EntityID id) {
  return UICollectionHolder::getEntityForIDEnforce(id);
}

static void advance_tracks(ImmTestHarness &h, float dt) {
  h.coll.merge_entity_arrays();
  for (const auto &e : h.coll.get_entities())
    if (e && e->has<motion::HasTracks>())
      e->get<motion::HasTracks>().advance(motion::scaled_dt(dt));
}

static void present_exiting(ImmTestHarness &h) {
  ui_motion::PresentExitingWidgets sys;
  h.coll.merge_entity_arrays();
  for (const auto &e : h.coll.get_entities())
    if (e && e->has<HasExitPresence>())
      sys.for_each_with(*e, e->get<HasExitPresence>(), 0.f);
}

static ComponentConfig row_config() {
  return ComponentConfig{}
      .with_size(ComponentSize{pixels(120), pixels(40)})
      .with_debug_name("exit_row")
      .with(ui_motion::on_appear(
          {.opacity = {0.f, 1.f}},
          motion::Timeline{.keys = {{0.f, 0.f}, {0.2f, 1.f}}}))
      .with(ui_motion::on_exit(
          {.opacity = 0.f},
          motion::Timeline{.keys = {{0.f, 0.f}, {0.3f, 1.f}}}));
}

static void exit_frame(ImmTestHarness &h) {
  h.begin_frame();
  advance_tracks(h, 0.1f);
  present_exiting(h);
  h.layout_only();
  imm::retire_unbuilt_ui_elements();
  h.coll.cleanup();
}

TEST(exit_runs_to_completion_then_widget_is_gone) {
  ui_motion::register_bridge<UIContext<ui_test::TestInputAction>>();
  ImmTestHarness h;
  auto built = div(h.context(), mk(h.root(), 0), row_config());
  const EntityID id = built.ent().id;
  auto child = div(h.context(), mk(built.ent(), 0),
                   ComponentConfig{}
                       .with_size(ComponentSize{pixels(40), pixels(20)})
                       .with_debug_name("exit_child"));
  const EntityID child_id = child.ent().id;
  h.layout_only();
  CHECK(h.find("exit_row") != nullptr);
  CHECK_APPROX(ent(id).get<HasOpacity>().value, 0.f);

  bool saw_mid_exit = false;
  for (int i = 0; i < 20 && alive(h, id); i++) {
    exit_frame(h);
    if (!alive(h, id))
      break;
    CHECK(h.find("exit_row")->was_rendered_to_screen);
    const float opacity = ent(id).get<HasOpacity>().value;
    if (opacity > 0.05f && opacity < 0.95f)
      saw_mid_exit = true;
  }
  CHECK(saw_mid_exit);
  CHECK(!alive(h, id));
  CHECK(!alive(h, child_id));
}

TEST(instant_motion_retires_without_exit_frames) {
  ui_motion::register_bridge<UIContext<ui_test::TestInputAction>>();
  ImmTestHarness h;
  auto built = div(h.context(), mk(h.root(), 0), row_config());
  const EntityID id = built.ent().id;
  h.layout_only();
  motion::set_instant(true);
  exit_frame(h);
  exit_frame(h);
  motion::set_instant(false);
  CHECK(!alive(h, id));
}

static EntityID build_row(ImmTestHarness &h) {
  return div(h.context(), mk(h.root(), 0), row_config()).ent().id;
}

TEST(rebuild_mid_exit_cancels_it) {
  ui_motion::register_bridge<UIContext<ui_test::TestInputAction>>();
  ImmTestHarness h;
  const EntityID id = build_row(h);
  h.layout_only();
  exit_frame(h);
  CHECK(alive(h, id));
  h.begin_frame();
  build_row(h);
  imm::retire_unbuilt_ui_elements();
  h.coll.cleanup();
  CHECK(alive(h, id));
  CHECK(!ent(id).has<HasExitPresence>());
  CHECK(!ent(id).get<ui_motion::HasMotionState>().exiting);
}

int main() { return ui_test::run_registered_tests("exit_animation_test"); }
