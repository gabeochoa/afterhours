// hello — the shortest complete afterhours app.
//
// A real window, a real event loop, a clickable button, and a theme file that
// reloads on save. Everything below main() is the app; main() is one call.

#include <cstdio>

#include "../../../../ah.h"
#include "../../../../src/graphics.h"
#define AFTER_HOURS_IMM_UI
#include "../../../../src/plugins/ui.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

struct Hello : System<DefaultUIContext> {
  void for_each_with(Entity &entity, DefaultUIContext &ctx, float) override {
    if (button(ctx, mk(entity), "Hello World!"))
      printf("clicked\n");
  }
};

int main(int, char **) {
  // Writes hello.theme on first run; edit any color and save to see it apply.
  return ui::run<>({.title = "hello"},
                   std::make_unique<theme_io::HotReloadTheme>("hello.theme"),
                   std::make_unique<Hello>());
}
