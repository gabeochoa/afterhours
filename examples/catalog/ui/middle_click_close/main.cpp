#include <cstdio>
#include <string>
#include <vector>

#include "../../../../ah.h"
#include "../../../../src/graphics.h"
#define AFTER_HOURS_IMM_UI
#include "../../../../src/plugins/e2e_testing/platform_test_input.h"
#include "../../../../src/plugins/ui.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

struct Tabs : System<DefaultUIContext> {
  std::vector<std::string> open = {"main.cpp", "notes.md", "todo.txt"};

  void for_each_with(Entity &root, DefaultUIContext &ctx, float) override {
    auto strip = div(ctx, mk(root, 0),
                     ComponentConfig{}
                         .with_size({pixels(360), pixels(32)})
                         .with_flex_direction(FlexDirection::Row));
    for (size_t i = 0; i < open.size(); ++i) {
      auto tab = button(ctx, mk(strip.ent(), static_cast<int>(i)),
                        ComponentConfig{}
                            .with_size({pixels(120), pixels(32)})
                            .with_label(open[i]));
      if (ctx.is_middle_click(tab.id())) {
        open.erase(open.begin() + static_cast<long>(i));
        break;
      }
    }
  }
};

int main() {
  graphics::Config config;
  config.display = graphics::DisplayMode::Headless;
  config.width = 400;
  config.height = 100;
  if (!graphics::init(config))
    return 1;

  SystemManager systems;
  auto tabs = std::make_unique<Tabs>();
  Tabs &strip = *tabs;
  ui::setup_with_resolution<>(systems, {400, 100}, std::move(tabs));
  testing::platform_input::set_test_mode(true);
  systems.run(1.f / 60.f);

  testing::test_input::simulate_middle_click(180.f, 16.f);
  for (int frame = 0; frame < 4; ++frame) {
    systems.run(1.f / 60.f);
    testing::test_input::reset_frame();
  }

  std::string left;
  for (const auto &name : strip.open)
    left += name + " ";
  std::printf("middle-clicked the second tab; open tabs: %s\n", left.c_str());
  graphics::shutdown();
  return strip.open.size() == 2 && strip.open[1] == "todo.txt" ? 0 : 1;
}
