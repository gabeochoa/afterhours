#include <cstdio>
#include <string>

#include "../../../../ah.h"
#include "../../../../src/graphics.h"
#define AFTER_HOURS_IMM_UI
#include "../../../../src/plugins/e2e_testing/platform_test_input.h"
#include "../../../../src/plugins/ui.h"
#include "../../../../src/plugins/ui/text_input/component.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

struct FileFilter : System<DefaultUIContext> {
  std::string filter = "main";
  EntityID filter_box = -1;

  void for_each_with(Entity &root, DefaultUIContext &ctx, float) override {
    auto box = imm::text_input(ctx, mk(root, 0), filter,
                               ComponentConfig{}.with_size({pixels(240), pixels(32)}));
    filter_box = box.id();
    button(ctx, mk(root, 1),
           ComponentConfig{}.with_size({pixels(240), pixels(28)}).with_label("main.cpp"));
  }
};

int main() {
  graphics::Config config;
  config.display = graphics::DisplayMode::Headless;
  config.width = 300;
  config.height = 120;
  if (!graphics::init(config))
    return 1;

  input::add_singleton_components(EntityHelper::createEntity(),
                                  ui::default_keymap<DefaultAction>());
  SystemManager systems;
  input::register_update_systems(systems);
  auto view = std::make_unique<FileFilter>();
  FileFilter &filter = *view;
  ui::setup_with_resolution<>(systems, {300, 120}, std::move(view));
  testing::platform_input::set_test_mode(true);
  for (int frame = 0; frame < 3; ++frame)
    systems.run(1.f / 60.f);

  auto &ctx = *EntityHelper::get_singleton_cmp<DefaultUIContext>();
  testing::test_input::simulate_click(120.f, 16.f);
  for (int frame = 0; frame < 3; ++frame) {
    systems.run(1.f / 60.f);
    testing::test_input::reset_frame();
  }
  const bool editing = ctx.focus_in_subtree(filter.filter_box);

  testing::test_input::push_key(keys::DOWN);
  for (int frame = 0; frame < 3; ++frame) {
    systems.run(1.f / 60.f);
    testing::test_input::reset_frame();
  }

  const bool still_editing = editing && ctx.focus_in_subtree(filter.filter_box);
  std::printf("pressed Down in the filter box: focus %s\n",
              still_editing ? "stayed in the box" : "moved to the file row");
  graphics::shutdown();
  return still_editing ? 0 : 1;
}
