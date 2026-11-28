#include <cstdio>

#define AFTER_HOURS_EXAMPLE_UI
#include "../../../example_helper.h"

using namespace afterhours;
using namespace afterhours::examples;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

float styled_value = 0.5f;
float plain_value = 0.5f;

struct Sliders : System<DefaultUIContext> {
  void for_each_with(Entity &entity, DefaultUIContext &ctx, float) override {
    slider(ctx, mk(entity, 0), styled_value,
           ComponentConfig{}
               .with_size(ComponentSize{pixels(300), pixels(36)})
               .with_slider_style(SliderStyle{.track = Color{40, 40, 40, 255},
                                              .fill = Color{200, 30, 90, 255},
                                              .handle = Color{30, 200, 90, 255},
                                              .handle_size = pixels(16.f),
                                              .track_thickness = pixels(8.f)})
               .with_debug_name("styled"));
    slider(ctx, mk(entity, 1), plain_value,
           ComponentConfig{}
               .with_size(ComponentSize{pixels(300), pixels(36)})
               .with_debug_name("plain"));
  }
};

int main(int, char **) {
  Checks checks;
  auto app = make_logic_ui<Sliders>({1280, 720}, std::make_unique<Sliders>());
  app.run(2);
  UICollectionHolder::get().collection.merge_entity_arrays();

  int tracks = 0, fills = 0, handles = 0;
  for (const auto &e : UICollectionHolder::get().collection.get_entities()) {
    if (!e || !e->has<UIComponentDebug>()) continue;
    const std::string &name = e->get<UIComponentDebug>().name_value;
    if (name == "slider_track") ++tracks;
    if (name == "slider_fill") ++fills;
    if (name == "slider_handle") ++handles;
  }
  printf("tracks=%d fills=%d handles=%d (want 1 1 2)\n", tracks, fills, handles);
  checks.check(tracks == 1 && fills == 1 && handles == 2,
               "styled slider shows track/fill/handle");
  return checks.finish("slider_parts");
}
