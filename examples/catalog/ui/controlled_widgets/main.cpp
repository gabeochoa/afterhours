#include <cstdio>

#include "../../../../ah.h"
#define AFTER_HOURS_IMM_UI
#include "../../../../src/plugins/ui.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

enum struct InputAction { None, WidgetNext, WidgetBack, WidgetPress };

struct Settings {
  float volume = 0.8f;
  bool muted = false;
};
Settings settings;
int frame = 0;

struct SettingsScreen : System<UIContext<InputAction>> {
  void for_each_with(Entity &entity, UIContext<InputAction> &ctx,
                     float) override {
    if (frame == 2)
      settings = Settings{.volume = 0.25f, .muted = true};

    slider(ctx, mk(entity, 0), settings.volume,
                         ComponentConfig{}.with_size(
                             ComponentSize{pixels(300), pixels(40)}));
    toggle_switch(ctx, mk(entity, 1), settings.muted,
                  ComponentConfig{}.with_label("Muted"));

    printf("frame %d: volume %.2f muted %d\n", frame, settings.volume,
           settings.muted);
    ++frame;
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
  EntityHelper::registerSingleton<window_manager::ProvidesCurrentResolution>(
      root);
  root.addComponent<AutoLayoutRoot>();
  root.addComponent<UIComponentDebug>("root");
  root.addComponent<UIComponent>(root.id)
      .set_desired_width(screen_pct(1.f))
      .set_desired_height(screen_pct(1.f));

  SystemManager systems;
  enforce_singletons<InputAction>(systems);
  systems.register_update_system(std::make_unique<ClearUIComponentChildren>());
  systems.register_update_system(
      std::make_unique<BeginUIContextManager<InputAction>>());
  systems.register_update_system(std::make_unique<SettingsScreen>());
  systems.register_update_system(
      std::make_unique<EndUIContextManager<InputAction>>());

  for (int i = 0; i < 5; ++i)
    systems.run(1.f / 60.f);

  return settings.muted && settings.volume == 0.25f ? 0 : 1;
}
