#include <cstdio>
#include <string>

#include "../../../../ah.h"
#define AFTER_HOURS_IMM_UI
#include "../../../../src/plugins/ui.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

EntityID column = -1, last_row = -1, list = -1, last_line = -1;

struct Screen : System<DefaultUIContext> {
  void for_each_with(Entity &entity, DefaultUIContext &ctx, float) override {
    auto rows = div(ctx, mk(entity, 0),
                    ComponentConfig{}
                        .with_size(ComponentSize{pixels(200), children()})
                        .with_flex_direction(FlexDirection::Column)
                        .with_gap(pixels(6)));
    for (int i = 0; i < 6; ++i)
      last_row = div(ctx, mk(rows.ent(), i),
                     ComponentConfig{}.with_size(
                         ComponentSize{pixels(180), pixels(28)}))
                     .ent()
                     .id;
    column = rows.ent().id;

    auto lines = div(ctx, mk(entity, 1),
                     ComponentConfig{}
                         .with_size(ComponentSize{pixels(300), pixels(200)})
                         .with_flex_direction(FlexDirection::Column)
                         .with_overflow(Overflow::Scroll, Axis::Y));
    for (int i = 0; i < 40; ++i)
      last_line = div(ctx, mk(lines.ent(), i),
                      ComponentConfig{}.with_size(
                          ComponentSize{pixels(280), pixels(22.5f)}))
                      .ent()
                      .id;
    list = lines.ent().id;
  }
};

int main(int, char **) {
  UIStylingDefaults::get().set_grid_snapping(true);
  SystemManager systems;
  ui::setup_with_resolution(systems, {1920, 1080}, std::make_unique<Screen>());
  for (int frame = 0; frame < 2; ++frame)
    systems.run(1.f / 60.f);

  const auto rect = [](EntityID id) {
    return UICollectionHolder::getEntityForIDEnforce(id).get<UIComponent>().rect();
  };
  const auto c = rect(column), r = rect(last_row);
  const float column_end = c.y + c.height, row_end = r.y + r.height;
  printf("children() column ends at %.0f, its last row at %.0f\n", column_end,
         row_end);

  const auto &scroll =
      UICollectionHolder::getEntityForIDEnforce(list).get<HasScrollView>();
  const float line_end = rect(last_line).y + rect(last_line).height - rect(list).y;
  printf("scroll content %.1f, last line ends at %.1f\n",
         scroll.content_size.y, line_end);

  return row_end <= column_end && scroll.content_size.y >= line_end ? 0 : 1;
}
