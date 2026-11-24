#include <cstdio>
#include <vector>

#include "../../../../ah.h"
#define AFTER_HOURS_IMM_UI
#include "../../../../src/plugins/ui.h"
#include "../../../../src/plugins/ui/menu.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

enum struct InputAction { None, WidgetNext, WidgetBack, WidgetPress };

bool menu_open = true;
const Color hover{56, 73, 98, 255};

struct EditMenu : System<UIContext<InputAction>> {
  void for_each_with(Entity &entity, UIContext<InputAction> &ctx,
                     float) override {
    const std::vector<MenuItem> items{
        {.label = "Cut", .shortcut = "Cmd+X"},
        {.label = "Copy", .shortcut = "Cmd+C"},
        MenuItem::sep(),
        {.label = "Select All", .shortcut = "Cmd+A"},
    };
    context_menu(ctx, mk(entity, 0), items, Vector2Type{40, 40}, menu_open,
                 ComponentConfig{}
                     .with_size(ComponentSize{pixels(240), pixels(32)})
                     .with_padding(Padding::all(pixels(12)))
                     .with_background(Theme::Usage::Secondary)
                     .with_custom_hover_bg(hover));
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
  systems.register_update_system(std::make_unique<EditMenu>());
  systems.register_update_system(
      std::make_unique<EndUIContextManager<InputAction>>());
  systems.run(1.f / 60.f);
  auto &ui_entities = UICollectionHolder::get().collection;
  ui_entities.merge_entity_arrays();

  int rows = 0;
  for (const auto &e : ui_entities.get_entities()) {
    if (!e || !e->has<HasLabel>() || !e->has<HasColor>() ||
        !e->get<HasColor>().hover_color)
      continue;
    const auto &label = e->get<HasLabel>();
    printf("%-10s inset %.0fpx\n", label.label.c_str(),
           label.text_inset.value_or(Vector2Type{}).x);
    ++rows;
  }
  return rows == 3 ? 0 : 1;
}
