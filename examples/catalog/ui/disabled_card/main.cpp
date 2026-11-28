#include <cstdio>

#define AFTER_HOURS_EXAMPLE_UI
#define AFTER_HOURS_EXAMPLE_GRAPHICS
#define AFTER_HOURS_EXAMPLE_TEST_INPUT
#include "../../../example_helper.h"

using namespace afterhours;
using namespace afterhours::examples;
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

int main() {
  Checks checks;
  HeadlessUI<ExportCard> app(400, 200, std::make_unique<ExportCard>());
  if (!app.ok()) return 1;

  app.run();
  app.click_at(120.f, 32.f);
  const int while_disabled = app.screen().exports;
  app.screen().has_document = true;
  app.run();
  app.click_at(120.f, 32.f);

  std::printf("clicks while disabled: %d, after enabling: %d\n", while_disabled,
              app.screen().exports - while_disabled);
  checks.check(while_disabled == 0, "disabled card ignores clicks");
  checks.check(app.screen().exports == 1, "enabled card counts one click");
  return checks.finish("disabled_card");
}
