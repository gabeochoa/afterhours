#include "ui_test_harness.h"
#include <afterhours/src/plugins/ui/systems.h>
#include <afterhours/src/plugins/toast.h>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;
using ui_test::TestInputAction;

static void check_skipped_updates(bool batched) {
  ImmTestHarness h;
  BeginUIContextManager<TestInputAction> begin;
  auto build = [&] {
    h.begin_frame();
    begin.for_each_with(h.context_entity(), h.context(), 1.f / 60.f);
    div(h.context(), mk(h.root(), 0), ComponentConfig{}
        .with_size({pixels(200), pixels(40)}).with_label("Latest text"));
    div(h.context(), mk(h.root(), 1), ComponentConfig{}
        .with_size({pixels(200), pixels(40)}).with_label("Overlay text")
        .with_render_layer(100));
  };
  build();
  const auto expected_count = h.context().render_cmds.size();
  const auto expected = batched ? h.render_batched() : h.render();
  CHECK(!expected.empty());
  for (int i = 0; i < 12; ++i) {
    build();
    CHECK(h.context().render_cmds.size() == expected_count);
  }
  const auto actual = batched ? h.render_batched() : h.render();
  CHECK(actual.size() == expected.size());
  CHECK(std::count_if(actual.begin(), actual.end(), [](const DrawCall &c) {
    return c.text == "Latest text";
  }) == 1);
  CHECK(std::count_if(actual.begin(), actual.end(), [](const DrawCall &c) {
    return c.text == "Overlay text";
  }) == 1);
}

TEST(skipped_updates_retire_previous_commands_in_both_renderers) {
  check_skipped_updates(false);
  check_skipped_updates(true);
}

TEST(deferred_submissions_belong_to_the_current_update) {
  ImmTestHarness h;
  UIContext<TestInputAction> other;
  other.render_cmds.push_back({h.root().id, 7});
  h.context().render_cmds.push_back({h.root().id, 1});
  h.context().defer([&] {
    CHECK(h.context().render_cmds.empty());
    h.context().render_cmds.push_back({h.root().id, 42});
  });
  BeginUIContextManager<TestInputAction> begin;
  begin.for_each_with(h.context_entity(), h.context(), 0);
  CHECK(h.context().render_cmds.size() == 1);
  CHECK(other.render_cmds.size() == 1);
}

TEST(toasts_registered_before_ui_begin_submit_during_render) {
  ImmTestHarness h;
  EntityHelper::registerSingleton<UIContext<TestInputAction>>(h.context_entity());
  auto notice = toast::send_info(h.context(), "Saved");
  h.layout_only();
  SystemManager systems;
  toast::register_layout_systems<TestInputAction>(systems);
  systems.register_update_system([&](float dt) {
    BeginUIContextManager<TestInputAction> begin;
    begin.for_each_with(h.context_entity(), h.context(), dt);
  });
  auto &entities = EntityHelper::get_entities_for_mod();
  for (int i = 0; i < 3; ++i) {
    systems.tick(entities, 0.f);
    CHECK(h.context().render_cmds.empty());
  }
  systems.render(entities, 0.f);
  CHECK(std::count_if(h.context().render_cmds.begin(), h.context().render_cmds.end(),
      [&](const RenderInfo &cmd) { return cmd.id == notice.ent().id; }) == 1);
}

int main() { return ui_test::run_registered_tests("ui_update_lifecycle"); }
