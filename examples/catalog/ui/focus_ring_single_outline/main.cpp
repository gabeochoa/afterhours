#include <cstdio>

#define AFTER_HOURS_EXAMPLE_UI
#include "../../../example_helper.h"

using namespace afterhours;
using namespace afterhours::examples;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

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
  Checks checks;
  auto app = make_logic_ui<Screen, InputAction>({1280, 720},
                                                std::make_unique<Screen>());
  app.run();
  checks.check(outlines_drawn == 1, "single focus outline");
  return checks.finish("focus_ring_single_outline");
}
