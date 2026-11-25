#include "ui_test_harness.h"
using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;
using Action = ui_test::TestInputAction;
namespace {
struct Fixture {
  ImmTestHarness h;
  std::vector<std::string> calls;
  EntityID control_id = -1;
  void build(const std::string &consume_at) {
    auto &ctx = h.context();
    h.begin_frame();
    auto outer = div(ctx, mk(h.root(), 0),
                     ComponentConfig{}
                         .with_size({pixels(300), pixels(200)})
                         .with_shortcut(Action::MenuBack, [&](Entity &) {
                           calls.push_back("outer");
                           return consume_at == "outer";
                         }));
    auto inner = div(ctx, mk(outer.ent(), 0),
                     ComponentConfig{}
                         .with_size({pixels(200), pixels(100)})
                         .with_shortcut(Action::MenuBack, [&](Entity &) {
                           calls.push_back("inner");
                           return consume_at == "inner";
                         }));
    auto control = button(ctx, mk(inner.ent(), 0),
                          ComponentConfig{}
                              .with_size({pixels(100), pixels(40)})
                              .with_label("OK")
                              .with_shortcut(Action::MenuBack, [&](Entity &) {
                                calls.push_back("control");
                                return consume_at == "control";
                              }));
    control_id = control.id();
    h.layout_only();
    ctx.set_focus(control_id);
  }
  bool dispatch(Action a = Action::MenuBack) {
    auto &ctx = h.context();
    ctx.last_action = a;
    DispatchShortcuts<Action> sys;
    sys.for_each_with(h.context_entity(), ctx, .016f);
    return ctx.last_action == Action::None;
  }
};
}
TEST(focused_control_gets_the_shortcut_first_and_consume_stops_routing) {
  Fixture f;
  f.build("control");
  CHECK(f.dispatch());
  CHECK(f.calls == std::vector<std::string>({"control"}));
  CHECK(!f.h.context().pressed(Action::MenuBack));
}
TEST(unconsumed_shortcut_bubbles_to_enclosing_scopes_in_order) {
  Fixture f;
  f.build("outer");
  CHECK(f.dispatch());
  CHECK(f.calls ==
        std::vector<std::string>({"control", "inner", "outer"}));
}
TEST(inner_scope_consume_stops_before_outer) {
  Fixture f;
  f.build("inner");
  CHECK(f.dispatch());
  CHECK(f.calls == std::vector<std::string>({"control", "inner"}));
}
TEST(nobody_consuming_leaves_the_action_for_the_app) {
  Fixture f;
  f.build("");
  CHECK(!f.dispatch());
  CHECK(f.calls ==
        std::vector<std::string>({"control", "inner", "outer"}));
  CHECK(f.h.context().pressed(Action::MenuBack));
  CHECK(!f.h.context().pressed(Action::MenuBack));
}
TEST(shortcuts_for_other_actions_do_not_fire) {
  Fixture f;
  f.build("control");
  CHECK(!f.dispatch(Action::WidgetPress));
  CHECK(f.calls.empty());
  CHECK(f.h.context().pressed(Action::WidgetPress));
}
TEST(a_gated_controls_handlers_are_skipped_and_routing_bubbles) {
  Fixture f;
  f.build("inner");
  const EntityID control = f.control_id;
  f.h.context().add_input_gate("test-modal",
                                [control](EntityID id) { return id != control; });
  CHECK(f.dispatch());
  CHECK(f.calls == std::vector<std::string>({"inner"}));
}
TEST(no_focus_means_no_dispatch) {
  Fixture f;
  f.build("control");
  f.h.context().set_focus(f.h.context().ROOT);
  CHECK(!f.dispatch());
  CHECK(f.calls.empty());
}
TEST(with_shortcut_lands_on_the_entity_and_clears_with_the_config) {
  ImmTestHarness h;
  auto build = [&](bool with_shortcut) -> Entity & {
    h.begin_frame();
    auto cfg = ComponentConfig{};
    if (with_shortcut)
      cfg.with_shortcut(Action::MenuBack, [](Entity &) { return true; });
    auto result = button(h.context(), mk(h.root(), 0), cfg);
    h.layout_only();
    return result.ent();
  };
  Entity &with_it = build(true);
  CHECK(with_it.has<HasShortcuts>());
  CHECK(with_it.get<HasShortcuts>().handles(
      static_cast<int>(Action::MenuBack)));
  Entity &without = build(false);
  CHECK(&without == &with_it);
  CHECK(!without.has<HasShortcuts>());
}
int main() { return ui_test::run_registered_tests("focus shortcuts"); }
