// layout_inspector_test.cpp
// Regression tests for LayoutInspector: it looked entities up in the default
// collection while UI entities live in UICollectionHolder (every pick said
// "Component not found"), and its panel maths read a screen_bounds field
// UIContext does not have (the system did not compile once instantiated).

#include "ui_test_harness.h"

#include <afterhours/src/plugins/ui/debug_overlay.h>
#include <afterhours/src/plugins/ui/layout_inspector.h>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;
using ui_test::TestInputAction;

// Picking a built widget resolves its debug name through the inspector.
TEST(layout_inspector_finds_ui_collection_entities) {
  ImmTestHarness h;
  div(h.context(), mk(h.root(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(120), pixels(40)})
          .with_debug_name("inspect_me"));
  h.layout_only();

  UIComponent *cmp = h.find("inspect_me");
  CHECK(cmp != nullptr);
  if (!cmp)
    return;

  LayoutInspector<TestInputAction> inspector(TestInputAction::None);
  CHECK(inspector.get_component_name(cmp->id) == "inspect_me");
  CHECK(inspector.get_component_name(999999) == "entity_999999");
}

// Constructing the debug systems instantiates their drawing paths.
TEST(layout_debug_systems_instantiate) {
  LayoutInspector<TestInputAction> inspector(TestInputAction::None);
  LayoutDebugOverlay<TestInputAction> overlay(TestInputAction::None);
  CHECK(!inspector.enabled);
  CHECK(!overlay.enabled);
}

int main() { return ui_test::run_registered_tests("layout_inspector tests"); }
