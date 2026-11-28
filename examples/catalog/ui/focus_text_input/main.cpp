#include <cstdio>

#define AFTER_HOURS_EXAMPLE_UI
#define AFTER_HOURS_EXAMPLE_GRAPHICS
#include "../../../example_helper.h"
#include "../../../../src/plugins/ui/text_input/component.h"

using namespace afterhours;
using namespace afterhours::examples;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

struct RenameDialog : System<DefaultUIContext> {
  bool open = false;
  bool just_opened = false;
  std::string title = "Untitled";
  EntityID field_id = -1;

  void for_each_with(Entity &root, DefaultUIContext &ctx, float) override {
    button(ctx, mk(root, 0), "Rename");
    if (!open)
      return;
    auto field = imm::text_input(ctx, mk(root, 1), title,
                                 ComponentConfig{}.with_size(
                                     {pixels(240), pixels(36)}));
    field_id = field.id();
    if (just_opened)
      ctx.set_focus(field.id());
    just_opened = false;
  }
};

int main() {
  Checks checks;
  HeadlessUI<RenameDialog> app(400, 300, std::make_unique<RenameDialog>());
  if (!app.ok()) return 1;

  for (int frame = 0; frame < 4; ++frame) {
    if (frame == 2)
      app.screen().open = app.screen().just_opened = true;
    app.run();
  }

  const auto &state =
      UICollectionHolder::getEntityForIDEnforce(app.screen().field_id)
          .get<text_input::HasTextInputState>();
  std::printf("rename field focused two frames after opening: %s\n",
              state.is_focused ? "yes" : "no");
  checks.check(state.is_focused, "field focused after opening");
  return checks.finish("focus_text_input");
}
