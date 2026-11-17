#include <cstdio>
#include <string>
#include <vector>

#include "../../../../ah.h"
#include "../../../../src/graphics.h"
#define AFTER_HOURS_IMM_UI
#include "../../../../src/plugins/e2e_testing/e2e_testing.h"
#include "../../../../src/plugins/e2e_testing/platform_test_input.h"
#include "../../../../src/plugins/ui.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

struct TabStrip : System<DefaultUIContext> {
  std::vector<std::string> tabs = {"A", "B", "C", "D"};

  void for_each_with(Entity &root, DefaultUIContext &ctx, float) override {
    auto *drag = EntityHelper::get_singleton_cmp<DragGroupState>();
    for (const auto &event : drag->events) {
      const std::string moved = tabs[static_cast<size_t>(event.source_index)];
      tabs.erase(tabs.begin() + event.source_index);
      tabs.insert(tabs.begin() + event.target_index, moved);
    }
    drag->events.clear();

    auto viewport = div(ctx, mk(root, 0),
                        ComponentConfig{}
                            .with_size({pixels(200), pixels(60)})
                            .with_overflow(Overflow::Scroll));
    auto strip = div(ctx, mk(viewport.ent(), 0),
                     ComponentConfig{}
                         .with_size({pixels(400), pixels(60)})
                         .with_flex_direction(FlexDirection::Row)
                         .with_draggable_children());
    for (size_t i = 0; i < tabs.size(); ++i)
      div(ctx, mk(strip.ent(), static_cast<int>(i)),
          ComponentConfig{}.with_size({pixels(100), pixels(60)}).with_label(tabs[i]));
    auto &scroll = viewport.ent().get<HasScrollView>();
    scroll.scroll_offset.x = scroll.scroll_target.x = 100;
  }
};

int main() {
  graphics::Config config;
  config.display = graphics::DisplayMode::Headless;
  config.width = 400;
  config.height = 200;
  if (!graphics::init(config))
    return 1;

  SystemManager systems;
  auto strip = std::make_unique<TabStrip>();
  TabStrip &tabs = *strip;
  ui::setup_with_resolution<>(systems, {400, 200}, std::move(strip));
  testing::register_all_handlers(systems);

  testing::platform_input::set_test_mode(true);
  testing::E2ERunner runner;
  runner.load_script(SCRIPT_PATH);
  while (!runner.is_finished()) {
    testing::test_input::reset_frame();
    runner.tick(1.f / 60.f);
    systems.run(1.f / 60.f);
  }

  std::string order;
  for (const auto &t : tabs.tabs)
    order += t;
  std::printf("dragged the tab drawn at x=150 (C) left: order %s\n", order.c_str());
  graphics::shutdown();
  return runner.has_failed() || order != "ACBD" ? 1 : 0;
}
