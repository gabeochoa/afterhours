#include <cstdio>

#include "../../../../ah.h"
#define AFTER_HOURS_IMM_UI
#include "../../../../src/plugins/ui.h"
#include "../../../../src/plugins/ui/disclosure.h"

using namespace afterhours;
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
  SystemManager systems;
  ui::setup_with_resolution(systems, {1280, 720}, std::make_unique<Panel>());
  systems.run(1.f / 60.f);
  systems.run(1.f / 60.f);
  printf("body_builds=%d\n", body_builds);
  return body_builds > 0 ? 0 : 1;
}
