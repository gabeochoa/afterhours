#include <cstdio>
#include <string>
#include <vector>

#define AFTER_HOURS_EXAMPLE_UI
#define AFTER_HOURS_EXAMPLE_GRAPHICS
#define AFTER_HOURS_EXAMPLE_TEST_INPUT
#include "../../../example_helper.h"

using namespace afterhours;
using namespace afterhours::examples;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

struct Tabs : System<DefaultUIContext> {
  std::vector<std::string> open = {"main.cpp", "notes.md", "todo.txt"};

  void for_each_with(Entity &root, DefaultUIContext &ctx, float) override {
    auto strip = div(ctx, mk(root, 0),
                     ComponentConfig{}
                         .with_size({pixels(360), pixels(32)})
                         .with_flex_direction(FlexDirection::Row));
    for (size_t i = 0; i < open.size(); ++i) {
      auto tab = button(ctx, mk(strip.ent(), static_cast<int>(i)),
                        ComponentConfig{}
                            .with_size({pixels(120), pixels(32)})
                            .with_label(open[i]));
      if (ctx.is_middle_click(tab.id())) {
        open.erase(open.begin() + static_cast<long>(i));
        break;
      }
    }
  }
};

int main() {
  Checks checks;
  HeadlessUI<Tabs> app(400, 100, std::make_unique<Tabs>());
  if (!app.ok())
    return 1;
  app.run();

  testing::test_input::simulate_middle_click(180.f, 16.f);
  for (int frame = 0; frame < 4; ++frame) {
    app.run(1);
    testing::test_input::reset_frame();
  }

  std::string left;
  for (const auto &name : app.screen().open)
    left += name + " ";
  std::printf("middle-clicked the second tab; open tabs: %s\n", left.c_str());
  checks.check(app.screen().open.size() == 2, "middle click closed one tab");
  checks.check(app.screen().open[1] == "todo.txt", "second tab was removed");
  return checks.finish("middle_click_close");
}
