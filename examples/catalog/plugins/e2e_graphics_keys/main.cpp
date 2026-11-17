#include <cstdio>
#include <format>

#include "../../../../ah.h"
#include "../../../../src/graphics.h"
#include "../../../../src/plugins/e2e_testing/e2e_testing.h"

using namespace afterhours;
using namespace afterhours::testing;

struct SaveShortcut : System<> {
  int saves = 0;

  bool should_iterate() const override { return false; }
  void once(float) override {
    if (graphics::is_key_down(keys::LEFT_SUPER) &&
        graphics::is_key_pressed(keys::S))
      saves++;
    auto &text = VisibleTextRegistry::instance();
    text.clear();
    text.register_text(std::format("saves: {}", saves));
  }
};

int main() {
  E2ERunner runner;
  runner.load_script(SCRIPT_PATH);

  SystemManager systems;
  systems.register_update_system(std::make_unique<SaveShortcut>());
  register_all_handlers(systems);

  const float dt = 1.f / 60.f;
  while (!runner.is_finished()) {
    test_input::reset_frame();
    runner.tick(dt);
    systems.run(dt);
  }
  runner.print_results();
  return runner.has_failed() ? 1 : 0;
}
