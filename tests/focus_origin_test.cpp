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

int main() { return ui_test::run_registered_tests("focus origin tests"); }
