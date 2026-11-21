
#include "ui_test_harness.h"
#include <afterhours/src/plugins/ui/systems.h>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

static void run_layout(ImmTestHarness &h, bool gated) {
  if (!EntityHelper::has_singleton<UIEntityMappingCache>()) {
    Entity &e = EntityHelper::createPermanentEntity();
    e.addComponent<UIEntityMappingCache>();
    EntityHelper::registerSingleton<UIEntityMappingCache>(e);
  }
  h.coll.merge_entity_arrays();
  BuildUIEntityMapping mapping;
  mapping.once(0.f);
  RunAutoLayout layout;
  layout.only_if_invalidated = gated;
  layout.once(0.f);
  layout.for_each_with(h.root(), h.root().get<AutoLayoutRoot>(),
                       h.root().get<UIComponent>(), 0.f);
}

TEST(mutate_after_layout_reads_fresh_same_frame) {
  ImmTestHarness h;
  auto host = div(h.context(), mk(h.root(), 0),
                  ComponentConfig{}.with_size({pixels(200), pixels(200)}));
  auto a = div(h.context(), mk(host.ent(), 0),
               ComponentConfig{}.with_size({pixels(200), pixels(50)}));
  auto b = div(h.context(), mk(host.ent(), 1),
               ComponentConfig{}.with_size({pixels(200), pixels(30)}));

  run_layout(h, false);
  CHECK_APPROX(b.cmp().rect().y, 50.f);
  CHECK(!layout_is_stale());

  const auto before = layout_generation();
  a.cmp().set_desired_height(pixels(90));
  a.cmp().invalidate_layout();
  CHECK(layout_generation() == before + 1);
  CHECK(layout_is_stale());
  CHECK_APPROX(b.cmp().rect().y, 50.f);

  run_layout(h, true);
  CHECK_APPROX(a.cmp().rect().height, 90.f);
  CHECK_APPROX(b.cmp().rect().y, 90.f);
  CHECK(!layout_is_stale());
}

TEST(no_invalidate_defers_and_stale_signal_stays_clear) {
  ImmTestHarness h;
  auto host = div(h.context(), mk(h.root(), 0),
                  ComponentConfig{}.with_size({pixels(200), pixels(200)}));
  auto a = div(h.context(), mk(host.ent(), 0),
               ComponentConfig{}.with_size({pixels(200), pixels(50)}));
  auto b = div(h.context(), mk(host.ent(), 1),
               ComponentConfig{}.with_size({pixels(200), pixels(30)}));

  run_layout(h, false);
  a.cmp().set_desired_height(pixels(90));
  run_layout(h, true);
  CHECK_APPROX(b.cmp().rect().y, 50.f);
  CHECK(!layout_is_stale());

  h.context().invalidate_layout();
  CHECK(h.context().layout_is_stale());
  run_layout(h, true);
  CHECK_APPROX(b.cmp().rect().y, 90.f);
  CHECK(!h.context().layout_is_stale());
}

int main() { return ui_test::run_registered_tests("layout_invalidate"); }
