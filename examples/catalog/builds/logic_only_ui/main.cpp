#include <cstdio>

#define AFTER_HOURS_EXAMPLE_UI
#include "../../../example_helper.h"

using namespace afterhours;
using namespace afterhours::examples;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

EntityID save_button = -1;

struct Menu : System<DefaultUIContext> {
  void for_each_with(Entity &entity, DefaultUIContext &ctx, float) override {
    auto column = div(ctx, mk(entity, 0),
                      ComponentConfig{}
                          .with_size(ComponentSize{pixels(300), children()})
                          .with_flex_direction(FlexDirection::Column)
                          .with_gap(pixels(8)));
    button(ctx, mk(column.ent(), 0),
           ComponentConfig{}
               .with_label("New game")
               .with_size(ComponentSize{pixels(300), pixels(40)}));
    save_button = button(ctx, mk(column.ent(), 1),
                         ComponentConfig{}
                             .with_label("Save")
                             .with_size(ComponentSize{pixels(300), pixels(40)}))
                      .ent()
                      .id;
  }
};

int main() {
  Checks checks;
  auto app = make_logic_ui<Menu>({1280, 720}, std::make_unique<Menu>());
  app.run(2);

  const auto rect =
      UICollectionHolder::getEntityForIDEnforce(save_button).get<UIComponent>().rect();
  std::printf("Save button laid out at y=%.0f, %.0fx%.0f, with no window\n",
              rect.y, rect.width, rect.height);
  checks.check(rect.y == 48.f && rect.height == 40.f,
               "Save button is laid out below New game");
  return checks.finish("logic_only_ui");
}
