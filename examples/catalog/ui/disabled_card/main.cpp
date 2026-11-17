#include <cstdio>

#include "../../../../ah.h"
#include "../../../../src/graphics.h"
#define AFTER_HOURS_IMM_UI
#include "../../../../src/plugins/e2e_testing/platform_test_input.h"
#include "../../../../src/plugins/ui.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

struct ExportCard : System<DefaultUIContext> {
  bool has_document = false;
  int exports = 0;

  void for_each_with(Entity &root, DefaultUIContext &ctx, float) override {
    auto card = div(ctx, mk(root, 0),
                    ComponentConfig{}
                        .with_size({pixels(240), pixels(64)})
                        .with_custom_background(colors::UI_BLACK)
                        .with_disabled(!has_document));
    card.ent().addComponentIfMissing<HasClickListener>([this](Entity &) { ++exports; });
    div(ctx, mk(card.ent(), 0),
        ComponentConfig{}.with_size({pixels(200), pixels(24)}).with_label("Export"));
    div(ctx, mk(card.ent(), 1),
        ComponentConfig{}.with_size({pixels(200), pixels(20)}).with_label("PDF, 3 pages"));
  }
};

static void click_card(SystemManager &systems) {
  testing::test_input::simulate_click(120.f, 32.f);
  for (int frame = 0; frame < 3; ++frame) {
    testing::test_input::reset_frame();
    systems.run(1.f / 60.f);
  }
}

int main() {
  graphics::Config config;
  config.display = graphics::DisplayMode::Headless;
  config.width = 400;
  config.height = 200;
  if (!graphics::init(config))
    return 1;

  SystemManager systems;
  auto card = std::make_unique<ExportCard>();
  ExportCard &screen = *card;
  ui::setup_with_resolution<>(systems, {400, 200}, std::move(card));
  testing::platform_input::set_test_mode(true);
  systems.run(1.f / 60.f);

  click_card(systems);
  const int while_disabled = screen.exports;
  screen.has_document = true;
  systems.run(1.f / 60.f);
  click_card(systems);

  std::printf("clicks while disabled: %d, after enabling: %d\n", while_disabled,
              screen.exports - while_disabled);
  graphics::shutdown();
  return while_disabled == 0 && screen.exports == 1 ? 0 : 1;
}
