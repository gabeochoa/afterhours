#pragma once

// Per-game e2e wiring: args, handler order, runner tick, screenshots, exit
// code. Handlers tick every frame via tick(), even while gameplay is paused.

#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <string>

#include "../../drawing_helpers.h"
#include "../../random_engine.h"
#include "e2e_testing.h"
#include "harness.h"
#include "platform_test_input.h"
#include "runner.h"
#include "test_input.h"

namespace afterhours {
namespace testing {

struct E2EApp {
  E2EArgs args;
  E2ERunner runner;
  SystemManager handlers;
  // Where screenshots go when --screenshot-dir is not given.
  std::string default_screenshot_dir;
  std::string pending_screenshot;

  // nullptr without --e2e or with no scripts. register_custom adds the
  // game's handlers between the builtins and the unknown-command handler.
  static std::unique_ptr<E2EApp>
  create(int argc, char *argv[],
         const std::function<void(SystemManager &)> &register_custom = {},
         const std::string &default_screenshot_dir = "") {
    if (!should_run_e2e(argc, argv))
      return nullptr;

    auto app = std::make_unique<E2EApp>();
    app->args = parse_e2e_args(argc, argv);
    app->default_screenshot_dir = default_screenshot_dir;
    // The input plugin only consults injected input while test_mode is on.
    platform_input::set_test_mode(true);
    configure_runner(app->runner, app->args);

    E2EApp *self = app.get();
    register_builtin_handlers(app->handlers);
    app->handlers.register_update_system(
        std::make_unique<HandleScreenshotCommand>(
            [self](const std::string &name) {
              self->pending_screenshot = name;
            }));
    app->handlers.register_update_system(
        std::make_unique<HandleResetTestStateCommand>(
            [self] { self->pending_screenshot.clear(); }));
    if (register_custom)
      register_custom(app->handlers);
    register_unknown_handler(app->handlers);
    register_cleanup(app->handlers);

    if (!app->args.script_dir.empty())
      app->runner.load_scripts_from_directory(app->args.script_dir);
    else if (!app->args.script_path.empty())
      app->runner.load_script(app->args.script_path);
    if (!app->runner.has_commands()) {
      log_error("e2e: no scripts found (dir '{}', file '{}')",
                app->args.script_dir, app->args.script_path);
      return nullptr;
    }
    return app;
  }

  // Opt in to repeatable runs: seeds RandomEngine now and at the start of
  // every script. Unset, RandomEngine stays randomly seeded.
  void set_seed(std::uint32_t seed) {
    RandomEngine::get().set_seed(seed);
    runner.set_reset_callback([seed] { RandomEngine::get().set_seed(seed); });
  }

  // Call before gameplay: dispatches the next command and runs the handlers,
  // so injected input lands in the same frame's gameplay.
  void tick(float dt) {
    test_input::reset_frame();
    runner.tick(dt);
    EntityHelper::merge_entity_arrays();
    handlers.tick_all(EntityHelper::get_entities_for_mod(), dt);
  }

  // Call after end_drawing: writes a pending `screenshot <name>` from the
  // game's render texture.
  void post_render(const graphics::RenderTextureType &target) {
    if (pending_screenshot.empty())
      return;
    const std::string name = std::move(pending_screenshot);
    pending_screenshot.clear();
    const std::filesystem::path path =
        screenshot_path(args, name, default_screenshot_dir);
    if (path.has_parent_path())
      std::filesystem::create_directories(path.parent_path());
    const auto png = capture_render_texture_png(target);
    if (!png) {
      log_warn("e2e: screenshot '{}' failed to capture", name);
      return;
    }
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char *>(png->bytes.data()),
              static_cast<std::streamsize>(png->bytes.size()));
    if (!out)
      log_warn("e2e: screenshot '{}' failed to write {}", name,
               path.string());
  }

  bool finished() const { return runner.is_finished(); }

  // Prints per-script results. 0 only if every script ran and passed.
  int finish() const {
    runner.print_results();
    return runner.is_finished() && !runner.has_failed() ? 0 : 1;
  }
};

} // namespace testing
} // namespace afterhours
