#include <cstdio>
#include <vector>

#define AFTER_HOURS_EXAMPLE_UI
#include "../../../example_helper.h"
#include "../../../../src/plugins/ui/menu.h"

using namespace afterhours;
using namespace afterhours::examples;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

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
  Checks checks;
  auto app = make_logic_ui<EditMenu, InputAction>({1280, 720},
                                                  std::make_unique<EditMenu>());
  app.run();
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
  checks.check(rows == 3, "three styled menu rows");
  return checks.finish("menu_styling");
}
