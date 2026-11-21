// layout_gen_ack_test.cpp
// Layout-generation acknowledgement (floatinghotel): a consumer that read a
// rect (or sampled a scroll anchor) needs to know which layout produced it
// and whether a later pass re-laid it out. Each widget carries the
// generation that last wrote its rect (0 = never laid out), so the read can
// be acknowledged by comparing stamps — including for a widget the global
// freshness signal alone cannot vouch for.

#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

TEST(laid_out_widgets_are_stamped_with_the_current_generation) {
  ImmTestHarness h;
  auto host = div(h.context(), mk(h.root(), 0),
                  ComponentConfig{}.with_size({pixels(200), pixels(200)}));
  auto a = div(h.context(), mk(host.ent(), 0),
               ComponentConfig{}.with_size({pixels(200), pixels(50)}));
  auto b = div(h.context(), mk(host.ent(), 1),
               ComponentConfig{}.with_size({pixels(200), pixels(30)}));

  h.layout_only();
  const auto root_gen = h.root().get<UIComponent>().laid_out_generation;
  CHECK(root_gen != 0);
  CHECK(a.cmp().laid_out_generation == root_gen);
  CHECK(b.cmp().laid_out_generation == root_gen);
  CHECK(a.cmp().laid_out_generation == laid_out_generation());
  CHECK(!layout_is_stale());

  // Ack a read, then invalidate without re-laying out: the stamp still
  // names the old layout, so the ack answers "not yet re-laid out".
  const auto read_gen = a.cmp().laid_out_generation;
  a.cmp().set_desired_height(pixels(90));
  invalidate_layout();
  CHECK(layout_is_stale());
  CHECK(!laid_out_since(a.cmp(), read_gen));

  h.layout_only();
  CHECK(laid_out_since(a.cmp(), read_gen));
  CHECK(a.cmp().laid_out_generation == laid_out_generation());
  CHECK(!layout_is_stale());
  CHECK_APPROX(a.cmp().rect().height, 90.f);
  const auto second_gen = a.cmp().laid_out_generation;
  CHECK(second_gen > read_gen);

  // Another pass with no invalidation still advances the stamp.
  h.layout_only();
  CHECK(a.cmp().laid_out_generation > second_gen);
}

TEST(never_laid_out_widget_reads_as_zero_despite_fresh_global_signal) {
  ImmTestHarness h;
  auto host = div(h.context(), mk(h.root(), 0),
                  ComponentConfig{}.with_size({pixels(200), pixels(200)}));
  auto a = div(h.context(), mk(host.ent(), 0),
               ComponentConfig{}.with_size({pixels(200), pixels(50)}));
  h.layout_only();
  CHECK(a.cmp().laid_out_generation != 0);

  // Built after the pass: nothing has laid it out, so its rect belongs to
  // no generation — the global signal would call it fresh.
  auto fresh = div(h.context(), mk(host.ent(), 1),
                   ComponentConfig{}.with_size({pixels(200), pixels(30)}));
  CHECK(fresh.cmp().laid_out_generation == 0);
  CHECK(!layout_is_stale());

  h.layout_only();
  CHECK(fresh.cmp().laid_out_generation != 0);
  CHECK(fresh.cmp().laid_out_generation == a.cmp().laid_out_generation);
}

int main() { return ui_test::run_registered_tests("layout_gen_ack"); }
