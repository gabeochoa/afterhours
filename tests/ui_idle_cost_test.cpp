// ui_idle_cost_test.cpp — R-277 residual: once-only System<> UI systems
// must not iterate. BuildUIEntityMapping and the two HandleDragGroups*
// systems do all their work in once(); their per-entity pass is the empty
// base for_each_with, so on an idle-UI world it visited every entity for
// nothing (2001 visits counted on a 2000-filler world before this fix).
// Same opt-out RevealKeyboardFocus / HotReloadTheme already use.

#include "ui_test_harness.h"
#include <afterhours/src/plugins/ui/systems.h>

using namespace afterhours;
using namespace afterhours::ui;
using ui_test::TestInputAction;

TEST(once_only_ui_systems_do_not_iterate) {
  CHECK(!BuildUIEntityMapping{}.should_iterate());
  CHECK(!HandleDragGroupsPreLayout<TestInputAction>{}.should_iterate());
  CHECK(!HandleDragGroupsPostLayout<TestInputAction>{}.should_iterate());
}

TEST(once_only_ui_systems_once_still_runs_without_singletons) {
  // No UIEntityMappingCache / DragGroupState registered: once() must
  // early-out, not crash — that is the idle-UI world this fix serves.
  BuildUIEntityMapping build;
  build.once(0.f);
  HandleDragGroupsPreLayout<TestInputAction> pre;
  pre.once(0.f);
  HandleDragGroupsPostLayout<TestInputAction> post;
  post.once(0.f);
  CHECK(true);
}

int main() { return ui_test::run_registered_tests("ui_idle_cost"); }
