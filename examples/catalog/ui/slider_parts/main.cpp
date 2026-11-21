#include <cstdio>

#include "../../../../ah.h"
#define AFTER_HOURS_IMM_UI
#include "../../../../src/plugins/ui.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

enum struct InputAction { None, WidgetNext, WidgetBack, WidgetPress };

float styled_value = 0.5f;
float plain_value = 0.5f;

struct Sliders : System<UIContext<InputAction>> {
  void for_each_with(Entity &entity, UIContext<InputAction> &ctx, float) override {
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
  auto &root = EntityHelper::createEntity();
  root.addComponent<UIContext<InputAction>>();
  EntityHelper::registerSingleton<UIContext<InputAction>>(root);
  root.addComponent<FontManager>()
      .load_font(UIComponent::DEFAULT_FONT, get_default_font())
      .load_font(UIComponent::SYMBOL_FONT, get_default_font())
      .load_font(UIComponent::UNSET_FONT, get_unset_font());
  EntityHelper::registerSingleton<FontManager>(root);
  root.addComponent<window_manager::ProvidesCurrentResolution>(
      window_manager::Resolution{1280, 720});
  EntityHelper::registerSingleton<window_manager::ProvidesCurrentResolution>(root);
  root.addComponent<AutoLayoutRoot>();
  root.addComponent<UIComponentDebug>("root");
  root.addComponent<UIComponent>(root.id)
      .set_desired_width(screen_pct(1.f))
      .set_desired_height(screen_pct(1.f));

  SystemManager systems;
  enforce_singletons<InputAction>(systems);
  systems.register_update_system(std::make_unique<ClearUIComponentChildren>());
  systems.register_update_system(std::make_unique<BeginUIContextManager<InputAction>>());
  systems.register_update_system(std::make_unique<Sliders>());
  systems.register_update_system(std::make_unique<EndUIContextManager<InputAction>>());
  systems.run(1.f / 60.f);
  systems.run(1.f / 60.f);
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
  return (tracks == 1 && fills == 1 && handles == 2) ? 0 : 1;
}
