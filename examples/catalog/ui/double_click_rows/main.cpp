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

struct Rows : System<DefaultUIContext> {
  std::vector<std::string> rows = {"alpha.txt", "beta.txt", "gamma.txt"};
  std::string opened;

  void for_each_with(Entity &root, DefaultUIContext &ctx, float) override {
    auto list = div(ctx, mk(root, 0),
                    ComponentConfig{}
                        .with_size({pixels(360), pixels(96)})
                        .with_flex_direction(FlexDirection::Column));
    for (size_t i = 0; i < rows.size(); ++i) {
      auto row = button(ctx, mk(list.ent(), static_cast<int>(i)),
                        ComponentConfig{}
                            .with_size({pixels(360), pixels(32)})
                            .with_label(rows[i]));
      if (ctx.is_double_click(row.id()))
        opened = rows[i];
    }
  }
};

int main() {
  Checks checks;
  HeadlessUI<Rows> app(400, 120, std::make_unique<Rows>());
  if (!app.ok()) return 1;

  app.run();
  app.click_at(180.f, 48.f);
  app.click_at(180.f, 48.f);

  std::printf("double-clicked the second row; opened: %s\n",
              app.screen().opened.c_str());
  checks.check(app.screen().opened == "beta.txt",
               "double-click opens second row");
  return checks.finish("double_click_rows");
}
