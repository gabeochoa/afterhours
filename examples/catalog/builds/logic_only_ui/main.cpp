#include <cstdio>

#include "../../../../ah.h"
#define AFTER_HOURS_IMM_UI
#include "../../../../src/plugins/ui.h"

using namespace afterhours;
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
  SystemManager systems;
  ui::setup_with_resolution(systems, {1280, 720}, std::make_unique<Menu>());
  for (int frame = 0; frame < 2; frame++)
    systems.run(1.f / 60.f);

  const auto rect =
      UICollectionHolder::getEntityForIDEnforce(save_button).get<UIComponent>().rect();
  std::printf("Save button laid out at y=%.0f, %.0fx%.0f, with no window\n",
              rect.y, rect.width, rect.height);
  return rect.y == 48.f && rect.height == 40.f ? 0 : 1;
}
