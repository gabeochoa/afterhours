#include <cstdio>

#include "../../../../ah.h"
#define AFTER_HOURS_IMM_UI
#include "../../../../src/plugins/ui.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

enum struct InputAction { None, WidgetNext, WidgetBack, WidgetPress };

int outlines_drawn = 0;

struct Screen : System<UIContext<InputAction>> {
  void for_each_with(Entity &entity, UIContext<InputAction> &ctx,
                     float) override {
    ctx.theme.focus_ring_thickness = 1.f;
    ctx.theme.focus_ring_contrast = false;

    auto save = button(ctx, mk(entity, 0),
                       ComponentConfig{}
                           .with_label("Save")
                           .with_size(ComponentSize{pixels(120), pixels(40)}));
    ctx.set_focus(save.ent().id);
    ctx.visual_focus_id = save.ent().id;

    if (auto ring = focus_ring_for(ctx, save.ent(), save.cmp())) {
      outlines_drawn = static_cast<int>(ring->thickness) +
                       (ring->contrast_edges ? 2 : 0);
      printf("ring %.0fpx, contrast edges %s: %d outline(s)\n",
             ring->thickness, ring->contrast_edges ? "on" : "off",
             outlines_drawn);
    }
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
  systems.register_update_system(std::make_unique<Screen>());
  systems.register_update_system(
      std::make_unique<EndUIContextManager<InputAction>>());
  systems.run(1.f / 60.f);

  return outlines_drawn == 1 ? 0 : 1;
}
