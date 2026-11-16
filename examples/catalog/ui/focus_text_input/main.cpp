#include <cstdio>

#include "../../../../ah.h"
#include "../../../../src/graphics.h"
#define AFTER_HOURS_IMM_UI
#include "../../../../src/plugins/ui.h"
#include "../../../../src/plugins/ui/text_input/component.h"

using namespace afterhours;
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
  graphics::Config config;
  config.display = graphics::DisplayMode::Headless;
  config.width = 400;
  config.height = 300;
  if (!graphics::init(config))
    return 1;

  SystemManager systems;
  auto dialog = std::make_unique<RenameDialog>();
  RenameDialog &rename = *dialog;
  ui::setup_with_resolution<>(systems, {400, 300}, std::move(dialog));

  for (int frame = 0; frame < 4; ++frame) {
    if (frame == 2)
      rename.open = rename.just_opened = true;
    systems.run(1.f / 60.f);
  }

  const auto &state = UICollectionHolder::getEntityForIDEnforce(rename.field_id)
                          .get<text_input::HasTextInputState>();
  std::printf("rename field focused two frames after opening: %s\n",
              state.is_focused ? "yes" : "no");
  graphics::shutdown();
  return state.is_focused ? 0 : 1;
}
