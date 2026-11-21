
#include <cmath>

#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

TEST(fast_op_never_shows) {
  DelayedBusy busy;
  busy.start();
  for (float dt : {0.05f, 0.05f, 0.05f, 0.05f}) {
    busy.update(dt);
    CHECK(!busy.visible());
  }
  busy.finish();
  CHECK(!busy.visible());
}

TEST(fast_op_builds_nothing) {
  ImmTestHarness h;
  DelayedBusy busy;
  busy.start();
  busy.update(0.1f);
  delayed_busy(h.context(), mk(h.root(), 0), busy, 0.5f,
               ComponentConfig{}
                   .with_size(ComponentSize{pixels(200), pixels(28)})
                   .with_debug_name("busy"));
  h.layout_only();
  CHECK(h.find("progress_track") == nullptr);
}

TEST(slow_op_shows_after_delay_then_hides) {
  DelayedBusy busy;
  busy.start();
  busy.update(0.2f);
  CHECK(!busy.visible());
  busy.update(0.051f);
  CHECK(busy.visible());

  ImmTestHarness h;
  auto result = delayed_busy(
      h.context(), mk(h.root(), 0), busy, 0.5f,
      ComponentConfig{}
          .with_size(ComponentSize{pixels(200), pixels(28)})
          .with_debug_name("busy"));
  h.layout_only();
  CHECK(h.find("progress_track") != nullptr);
  CHECK_APPROX(result.as<float>(), 0.5f);

  busy.finish();
  CHECK(!busy.visible());
}

TEST(min_visible_drains_on_completion) {
  DelayedBusy busy;
  busy.min_visible = 0.5f;
  busy.start();
  busy.update(0.25f);
  CHECK(busy.visible());
  busy.finish();
  CHECK(busy.visible());
  busy.update(0.4f);
  CHECK(busy.visible());
  busy.update(0.11f);
  CHECK(!busy.visible());

  DelayedBusy long_op;
  long_op.min_visible = 0.5f;
  long_op.start();
  long_op.update(2.0f);
  long_op.finish();
  CHECK(!long_op.visible());
}

TEST(restart_resets) {
  DelayedBusy busy;
  busy.min_visible = 0.5f;
  busy.start();
  busy.update(0.3f);
  CHECK(busy.visible());
  busy.finish();
  CHECK(busy.visible());
  busy.start();
  CHECK(!busy.visible());
  busy.update(0.1f);
  CHECK(!busy.visible());
}

TEST(bad_dt_ignored) {
  DelayedBusy busy;
  busy.start();
  busy.update(-5.f);
  busy.update(std::numeric_limits<float>::quiet_NaN());
  busy.update(std::numeric_limits<float>::infinity());
  CHECK(busy.elapsed() == 0.f);
  CHECK(!busy.visible());
}

TEST(indeterminate_sweep) {
  DelayedBusy busy;
  busy.start();
  busy.update(1.25f);
  CHECK(std::fabs(busy.indeterminate() - 0.25f) < 0.01f);
  busy.update(1.0f);
  CHECK(std::fabs(busy.indeterminate() - 0.25f) < 0.01f);
}

TEST(indeterminate_widget_branch) {
  ImmTestHarness h;
  DelayedBusy busy;
  busy.start();
  busy.update(1.25f);
  auto result = delayed_busy(
      h.context(), mk(h.root(), 0), busy, -1.f,
      ComponentConfig{}
          .with_size(ComponentSize{pixels(60), pixels(60)})
          .with_debug_name("busy_cp"));
  h.layout_only();
  UIComponent *cp = h.find("busy_cp");
  CHECK(cp != nullptr);
  if (cp) {
    auto ent = UICollectionHolder::getEntityForID(cp->id);
    CHECK(std::fabs(ent->get<HasCircularProgressState>().value - 0.25f) <
          0.01f);
  }
  CHECK_APPROX(result.as<float>(), 0.25f);
}

int main() { return ui_test::run_registered_tests("delayed_busy tests"); }
