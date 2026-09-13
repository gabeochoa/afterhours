// Focus records WHERE it was set, not just what holds it.
//
// puzzle added one focusable column and Tab stopped advancing. dump_ui says
// which widget has focus; nothing said what gave it focus, so finding the
// cause was guess-and-rebuild and they never found it.

#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using ui_test::ImmTestHarness;

TEST(focus_records_the_line_that_set_it) {
  ImmTestHarness h;
  h.context().set_focus(7);

  const std::string origin = h.context().focus_origin();
  CHECK(origin.find("focus_origin_test.cpp") != std::string::npos);
  CHECK(origin.find("Explicit") != std::string::npos);
}

TEST(focus_origin_names_the_source_kind) {
  ImmTestHarness h;
  h.context().set_focus(3, FocusSource::Pointer);
  CHECK(h.context().focus_origin().find("Pointer") != std::string::npos);
}

// A re-grab of the widget that already has focus is not news. try_to_grab runs
// every frame, so recording it would bury whatever actually moved focus.
TEST(re_setting_the_same_id_keeps_the_original_origin) {
  ImmTestHarness h;
  h.context().set_focus(11, FocusSource::Explicit);
  const std::string first = h.context().focus_origin();

  h.context().set_focus(11, FocusSource::Grab);
  CHECK(h.context().focus_origin() == first);
}

// Moving focus IS news, and the new writer is the one worth reporting.
TEST(moving_focus_records_the_new_writer) {
  ImmTestHarness h;
  h.context().set_focus(11, FocusSource::Explicit);
  h.context().set_focus(12, FocusSource::Grab);
  CHECK(h.context().focus_origin().find("Grab") != std::string::npos);
}

TEST(focus_never_set_says_so) {
  ImmTestHarness h;
  CHECK(h.context().focus_origin().find("never set") != std::string::npos);
}

// A ring painted at rest sits on whatever was focusable first, so an app opens
// with a box around a row nobody touched. 14 wm baselines had one.
TEST(nothing_is_ring_worthy_until_something_is_interacted_with) {
  ImmTestHarness h;
  CHECK(!h.context().has_interacted);

  // try_to_grab is the per-frame re-grab. It gives focus, but it is not intent.
  h.context().try_to_grab(7);
  CHECK(h.context().has_focus(7));
  CHECK(!h.context().has_interacted);
}

TEST(a_deliberate_focus_move_counts_as_interaction) {
  ImmTestHarness h;
  h.context().try_to_grab(7);
  CHECK(!h.context().has_interacted);

  h.context().set_focus(9, FocusSource::Explicit);
  CHECK(h.context().has_interacted);
}

TEST(pointer_focus_counts_too) {
  ImmTestHarness h;
  h.context().set_focus(3, FocusSource::Pointer);
  CHECK(h.context().has_interacted);
}

// Grabbing a different widget is still the framework re-grabbing, not a user.
TEST(a_grab_onto_a_new_widget_is_still_not_interaction) {
  ImmTestHarness h;
  h.context().set_focus(4, FocusSource::Grab);
  CHECK(!h.context().has_interacted);
}


TEST(unconsumed_action_expires_at_next_ui_frame) {
  ImmTestHarness h;
  auto &ctx = h.context();
  ctx.last_action = ui_test::TestInputAction::MenuBack;
  BeginUIContextManager<ui_test::TestInputAction> begin;
  begin.for_each_with(h.context_entity(), ctx, .016f);
  CHECK(!ctx.pressed(ui_test::TestInputAction::MenuBack));
  ctx.defer([&ctx] { ctx.last_action = ui_test::TestInputAction::WidgetPress; });
  begin.for_each_with(h.context_entity(), ctx, .016f);
  CHECK(ctx.pressed(ui_test::TestInputAction::WidgetPress));
  ctx.last_action = ui_test::TestInputAction::MenuBack;
  ctx.reset();
  CHECK(!ctx.pressed(ui_test::TestInputAction::MenuBack));
}

int main() { return ui_test::run_registered_tests("focus origin tests"); }
