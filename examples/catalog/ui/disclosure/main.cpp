#include <cstdio>

#define AFTER_HOURS_EXAMPLE_UI
#include "../../../example_helper.h"
#include "../../../../src/plugins/ui/disclosure.h"

using namespace afterhours;
using namespace afterhours::examples;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

int body_builds = 0;

struct Panel : System<DefaultUIContext> {
  void for_each_with(Entity &entity, DefaultUIContext &ctx, float) override {
    disclosure(ctx, mk(entity, 0), "Advanced options",
               [&](Entity &body_entity) {
                 body_builds++;
                 div(ctx, mk(body_entity, 0),
                     ComponentConfig{}
                         .with_size(ComponentSize{pixels(300), pixels(24)})
                         .with_label("Body shown while expanded"));
               },
               DisclosureConfig{.default_expanded = true},
               ComponentConfig{}.with_size(
                   ComponentSize{pixels(360), children()}));
  }
};

int main(int, char **) {
  Checks checks;
  auto app = make_logic_ui<Panel>({1280, 720}, std::make_unique<Panel>());
  app.run(2);

  printf("body_builds=%d\n", body_builds);
  checks.check(body_builds > 0, "expanded disclosure builds its body");
  return checks.finish("disclosure");
}
