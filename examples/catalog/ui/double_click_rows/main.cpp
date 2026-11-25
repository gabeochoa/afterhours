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

struct Rows : System<DefaultUIContext> {
  std::vector<std::string> rows = {"alpha.txt", "beta.txt", "gamma.txt"};
  std::string opened;

  void for_each_with(Entity &root, DefaultUIContext &ctx, float) override {
    auto list = div(ctx, mk(root, 0),
                    ComponentConfig{}
                        .with_size({pixels(360), pixels(96)})
                        .with_flex_direction(FlexDirection::Column));
    for (size_t i = 0; i < rows.size(); ++i) {
      auto row = button(ctx, mk(list.ent(), static_cast<int>(i)),
                        ComponentConfig{}
                            .with_size({pixels(360), pixels(32)})
                            .with_label(rows[i]));
      if (ctx.is_double_click(row.id()))
        opened = rows[i];
    }
  }
};

int main() {
  graphics::Config config;
  config.display = graphics::DisplayMode::Headless;
  config.width = 400;
  config.height = 120;
  if (!graphics::init(config))
    return 1;

  SystemManager systems;
  auto rows_sys = std::make_unique<Rows>();
  Rows &rows = *rows_sys;
  ui::setup_with_resolution<>(systems, {400, 120}, std::move(rows_sys));
  testing::platform_input::set_test_mode(true);
  systems.run(1.f / 60.f);

  for (int click = 0; click < 2; ++click) {
    testing::test_input::simulate_click(180.f, 48.f);
    for (int frame = 0; frame < 4; ++frame) {
      systems.run(1.f / 60.f);
      testing::test_input::reset_frame();
    }
  }

  std::printf("double-clicked the second row; opened: %s\n",
              rows.opened.c_str());
  graphics::shutdown();
  return rows.opened == "beta.txt" ? 0 : 1;
}
