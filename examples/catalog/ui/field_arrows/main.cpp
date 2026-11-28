#include <cstdio>
#include <string>

#define AFTER_HOURS_EXAMPLE_UI
#define AFTER_HOURS_EXAMPLE_GRAPHICS
#define AFTER_HOURS_EXAMPLE_TEST_INPUT
#include "../../../example_helper.h"
#include "../../../../src/plugins/ui/text_input/component.h"

using namespace afterhours;
using namespace afterhours::examples;
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
  Checks checks;
  input::add_singleton_components(EntityHelper::createEntity(),
                                  ui::default_keymap<DefaultAction>());
  HeadlessUI<FileFilter> app(300, 120, std::make_unique<FileFilter>(),
                             std::make_unique<input::InputSystem>());
  if (!app.ok()) return 1;
  app.run(3);

  auto &ctx = app.ui.context();
  app.click_at(120.f, 16.f);
  const bool editing = ctx.focus_in_subtree(app.screen().filter_box);

  testing::test_input::push_key(keys::DOWN);
  for (int frame = 0; frame < 3; ++frame) {
    app.run();
    testing::test_input::reset_frame();
  }

  const bool still_editing =
      editing && ctx.focus_in_subtree(app.screen().filter_box);
  std::printf("pressed Down in the filter box: focus %s\n",
              still_editing ? "stayed in the box" : "moved to the file row");
  checks.check(still_editing, "Down stays in the text box");
  return checks.finish("field_arrows");
}
