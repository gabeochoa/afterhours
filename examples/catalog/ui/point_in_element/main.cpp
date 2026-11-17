#include "../../../../ah.h"
#include "../../../../src/plugins/autolayout.h"
#include "../../../../src/plugins/e2e_testing/ui_commands.h"
#include "../../../../src/plugins/ui/component_init.h"
#include "../../../../src/plugins/ui/imm_components.h"

#include <cstdio>
#include <format>
#include <vector>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using afterhours::testing::PendingE2ECommand;
using afterhours::testing::ui_commands::HandleExpectPointInCommand;

enum struct ExampleInputAction {
  None,
  WidgetMod,
  WidgetNext,
  WidgetBack,
  WidgetPress,
  WidgetUp,
  WidgetDown,
  WidgetLeft,
  WidgetRight,
};

static bool run_expect(Entity &holder,
                       const std::vector<std::string> &args) {
  auto &cmd = holder.addComponent<PendingE2ECommand>();
  cmd.name = "expect_point_in";
  cmd.args = args;
  HandleExpectPointInCommand handler;
  handler.for_each_with(holder, cmd, 0.f);
  const bool passed = cmd.is_consumed() && cmd.succeeded();
  printf("expect_point_in %s %s %s -> %s%s\n", args[0].c_str(),
         args[1].c_str(), args[2].c_str(), passed ? "inside" : "not inside",
         cmd.last_mismatch.empty()
             ? ""
             : std::format(" ({})", cmd.last_mismatch).c_str());
  holder.removeComponent<PendingE2ECommand>();
  return passed;
}

int main() {
  EntityCollection &coll = UICollectionHolder::get().collection;
  imm::clear_existing_ui_elements();

  Entity &ctx_entity = coll.createEntity();
  auto &ctx = ctx_entity.addComponent<UIContext<ExampleInputAction>>();
  ctx.screen_width = 800;
  ctx.screen_height = 600;

  Entity &root = coll.createEntity();
  root.addComponent<UIComponent>(root.id);
  root.addComponent<AutoLayoutRoot>();
  root.get<UIComponent>().set_desired_width(pixels(800));
  root.get<UIComponent>().set_desired_height(pixels(600));

  div(ctx, mk(root, 0),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(200), pixels(100)})
          .with_debug_name("panel"));

  coll.merge_entity_arrays();
  auto &entities = coll.get_entities();
  EntityID max_id = 0;
  for (const auto &e : entities)
    if (e) max_id = std::max(max_id, e->id);
  std::vector<Entity *> mapping(static_cast<size_t>(max_id) + 1, nullptr);
  for (const auto &e : entities)
    if (e) mapping[static_cast<size_t>(e->id)] = e.get();
  AutoLayout::autolayout(root.get<UIComponent>(),
                         window_manager::Resolution{800, 600}, mapping, false,
                         1.0f);
  for (const auto &e : entities)
    if (e && e->has<UIComponent>()) e->get<UIComponent>().was_rendered_to_screen = true;

  Entity &holder = coll.createEntity();
  const bool inside = run_expect(holder, {"panel", "50", "50"});
  const bool outside = run_expect(holder, {"panel", "250", "50"});
  printf("%s\n", inside && !outside ? "ok" : "unexpected result");
  return inside && !outside ? 0 : 1;
}
