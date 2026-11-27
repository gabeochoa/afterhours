#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

TEST(empty_stack_is_todays_legacy_gates) {
  ui_test::ImmTestHarness h;
  CHECK(h.context().is_input_allowed(10));
  h.context().add_input_gate("legacy", [](EntityID id) { return id == 10; });
  CHECK(h.context().is_input_allowed(10));
  CHECK(!h.context().is_input_allowed(20));
}

TEST(pushed_gate_blocks_lower_layer_and_pop_restores) {
  ui_test::ImmTestHarness h;
  h.context().add_input_gate("legacy", [](EntityID) { return true; });
  auto token = h.context().push_input_gate(
      "overlay", [](EntityID id) { return id == 10; });
  CHECK(h.context().is_input_allowed(10));
  CHECK(!h.context().is_input_allowed(20));
  CHECK(h.context().pop_input_gate(token));
  CHECK(h.context().is_input_allowed(20));
  CHECK(!h.context().pop_input_gate(token));
}

TEST(nested_gates_are_lifo_and_top_owns_input) {
  ui_test::ImmTestHarness h;
  h.context().add_input_gate("legacy", [](EntityID) { return false; });
  h.context().push_input_gate("a", [](EntityID id) { return id == 10; });
  h.context().push_input_gate("b", [](EntityID id) { return id == 20; });
  CHECK(h.context().input_gate_depth() == 2);
  CHECK(h.context().top_input_gate_name() == "b");
  CHECK(!h.context().is_input_allowed(10));
  CHECK(h.context().is_input_allowed(20));
  CHECK(h.context().pop_input_gate());
  CHECK(h.context().is_input_allowed(10));
  CHECK(!h.context().is_input_allowed(20));
}

TEST(scoped_gate_pops_on_scope_exit_and_out_of_order_pop_keeps_top) {
  ui_test::ImmTestHarness h;
  auto outer = h.context().scoped_input_gate(
      "outer", [](EntityID id) { return id == 10; });
  {
    auto inner = h.context().scoped_input_gate(
        "inner", [](EntityID id) { return id == 20; });
    CHECK(h.context().is_input_allowed(20));
    outer.reset();
    CHECK(h.context().input_gate_depth() == 1);
    CHECK(h.context().is_input_allowed(20));
  }
  CHECK(h.context().input_gate_depth() == 0);
  CHECK(h.context().is_input_allowed(10));
}

TEST(entity_gate_allows_only_its_subtree) {
  ui_test::ImmTestHarness h;
  h.begin_frame();
  auto layer = div(h.context(), mk(h.root(), 0),
                   ComponentConfig{}.with_size({pixels(100), pixels(100)}));
  auto child = div(h.context(), mk(layer.ent(), 0),
                   ComponentConfig{}.with_size({pixels(50), pixels(50)}));
  auto outside = div(h.context(), mk(h.root(), 1),
                     ComponentConfig{}.with_size({pixels(100), pixels(100)}));
  auto guard = h.context().scoped_input_gate("layer", layer.ent().id);
  CHECK(h.context().is_input_allowed(layer.ent().id));
  CHECK(h.context().is_input_allowed(child.ent().id));
  CHECK(!h.context().is_input_allowed(outside.ent().id));
  CHECK(h.context().is_input_allowed(-1));
}

int main() { return ui_test::run_registered_tests("focus stack"); }
