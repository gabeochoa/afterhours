#include <cstdio>

#define AFTER_HOURS_EXAMPLE_UI
#include "../../../example_helper.h"

using namespace afterhours;
using namespace afterhours::examples;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

struct Settings {
  float volume = 0.8f;
  bool muted = false;
};
Settings settings;
int frame = 0;

struct SettingsScreen : System<UIContext<InputAction>> {
  void for_each_with(Entity &entity, UIContext<InputAction> &ctx,
                     float) override {
    if (frame == 2)
      settings = Settings{.volume = 0.25f, .muted = true};

    slider(ctx, mk(entity, 0), settings.volume,
                         ComponentConfig{}.with_size(
                             ComponentSize{pixels(300), pixels(40)}));
    toggle_switch(ctx, mk(entity, 1), settings.muted,
                  ComponentConfig{}.with_label("Muted"));

    printf("frame %d: volume %.2f muted %d\n", frame, settings.volume,
           settings.muted);
    ++frame;
  }
};

int main(int, char **) {
  Checks checks;
  auto app = make_logic_ui<SettingsScreen, InputAction>(
      {1280, 720}, std::make_unique<SettingsScreen>());
  app.run(5);
  checks.check(settings.muted && settings.volume == 0.25f,
               "settings survive controlled mutation");
  return checks.finish("controlled_widgets");
}
