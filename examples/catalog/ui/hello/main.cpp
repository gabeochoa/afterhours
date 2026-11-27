// hello — the shortest complete afterhours app.
//
// A real window, a real event loop, a clickable button, and a theme file that
// reloads on save. Everything below main() is the app; main() is one call.
//
// With AFTERHOURS_TEST_FRAMES=N set, the same UI runs headless for N frames
// and exits 0 only if the button was built every frame (used by `make test`).

#include <cstdio>
#include <cstdlib>

#include "../../../../ah.h"
#include "../../../../src/graphics.h"
#define AFTER_HOURS_IMM_UI
#include "../../../../src/plugins/ui.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

struct Hello : System<DefaultUIContext> {
  int frames = 0;
  void for_each_with(Entity &entity, DefaultUIContext &ctx, float) override {
    ++frames;
    if (button(ctx, mk(entity), "Hello World!"))
      printf("clicked\n");
  }
};

int main(int, char **) {
  if (const char *env = std::getenv("AFTERHOURS_TEST_FRAMES")) {
    const int test_frames = std::atoi(env);
    graphics::Config config;
    config.display = graphics::DisplayMode::Headless;
    if (!graphics::init(config))
      return 1;
    SystemManager systems;
    auto hello = std::make_unique<Hello>();
    Hello &screen = *hello;
    ui::setup_with_resolution<>(systems, {config.width, config.height},
                                std::move(hello));
    for (int i = 0; i < test_frames; ++i)
      systems.run(1.f / 60.f);
    graphics::shutdown();
    printf("hello ran %d/%d headless frames\n", screen.frames, test_frames);
    return test_frames > 0 && screen.frames == test_frames ? 0 : 1;
  }

  // Writes hello.theme on first run; edit any color and save to see it apply.
  return ui::run<>({.title = "hello"},
                   std::make_unique<theme_io::HotReloadTheme>("hello.theme"),
                   std::make_unique<Hello>());
}
