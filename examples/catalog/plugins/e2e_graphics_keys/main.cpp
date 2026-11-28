#define AFTER_HOURS_EXAMPLE_E2E
#include "../../../example_helper.h"

using namespace afterhours;
using namespace afterhours::examples;
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

int main(int argc, char *argv[]) {
  E2EHarness harness(argc, argv);
  if (!harness.has_script()) harness.args.script_path = SCRIPT_PATH;

  harness.add_systems(std::make_unique<SaveShortcut>());
  return harness.run([&] { test_input::reset_frame(); });
}
