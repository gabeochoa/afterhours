#include <cstdio>
#include <string>
#include <vector>

#define AFTER_HOURS_EXAMPLE_UI
#define AFTER_HOURS_EXAMPLE_GRAPHICS
#define AFTER_HOURS_EXAMPLE_TEST_INPUT
#include "../../../example_helper.h"
#include "../../../../src/plugins/e2e_testing/e2e_testing.h"

using namespace afterhours;
using namespace afterhours::examples;
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
  Checks checks;
  HeadlessUI<TabStrip> app(400, 200, std::make_unique<TabStrip>());
  if (!app.ok()) return 1;
  testing::register_all_handlers(app.ui.systems);

  testing::E2ERunner runner;
  runner.load_script(SCRIPT_PATH);
  while (!runner.is_finished()) {
    testing::test_input::reset_frame();
    runner.tick(1.f / 60.f);
    app.run();
  }

  std::string order;
  for (const auto &t : app.screen().tabs)
    order += t;
  std::printf("dragged the tab drawn at x=150 (C) left: order %s\n",
              order.c_str());
  checks.check(!runner.has_failed(), "e2e script passes");
  checks.check(order == "ACBD", "drag reorders the strip");
  return checks.finish("drag_scrolled_strip");
}
