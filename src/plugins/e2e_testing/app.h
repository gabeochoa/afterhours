#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <string>

#include "../../drawing_helpers.h"
#include "../../graphics.h"
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
  std::string default_screenshot_dir;
  std::string pending_screenshot;
  std::function<const graphics::RenderTextureType *()> screenshot_source;
  int exit_code = 0;

  static std::unique_ptr<E2EApp>
  create(int argc, char *argv[],
         const std::function<void(SystemManager &)> &register_custom = {},
         const std::string &default_screenshot_dir = "") {
    if (!should_run_e2e(argc, argv))
      return nullptr;
    auto app = std::make_unique<E2EApp>();
    app->args = parse_e2e_args(argc, argv);
    app->default_screenshot_dir = default_screenshot_dir;
    platform_input::set_test_mode(true);
    configure_runner(app->runner, app->args);
    E2EApp *self = app.get();
    register_builtin_handlers(app->handlers);
    app->handlers.register_update_system(
        std::make_unique<HandleScreenshotCommand>(
            [self](const std::string &name) {
              self->pending_screenshot = name;
              return true;
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

  static std::shared_ptr<E2EApp>
  attach(graphics::RunConfig &cfg, int argc, char *argv[],
         const std::function<void(SystemManager &)> &register_custom = {},
         const std::string &default_screenshot_dir = "") {
    if (!should_run_e2e(argc, argv))
      return nullptr;
    std::shared_ptr<E2EApp> app =
        create(argc, argv, register_custom, default_screenshot_dir);
    if (!app) {
      app = std::make_shared<E2EApp>();
      app->exit_code = 1;
      cfg.init = [init = cfg.init] {
        if (init)
          init();
        graphics::request_quit();
      };
      return app;
    }
    cfg.display = app->args.display_mode();
    if (cfg.fixed_dt <= 0.f)
      cfg.fixed_dt = 1.f / 60.f;
    cfg.frame = [app, frame = cfg.frame, dt = cfg.fixed_dt] {
      if (app->screenshot_source) {
        if (const auto *rt = app->screenshot_source())
          app->post_render(*rt);
      }
      if (app->finished()) {
        graphics::request_quit();
        return;
      }
      app->tick(dt);
      if (frame)
        frame();
    };
    cfg.cleanup = [app, cleanup = cfg.cleanup] {
      if (cleanup)
        cleanup();
      app->exit_code = app->finish();
    };
    return app;
  }

  void set_seed(std::uint32_t seed) {
    RandomEngine::get().set_seed(seed);
    runner.set_reset_callback([seed] { RandomEngine::get().set_seed(seed); });
  }

  void tick(float dt) {
    test_input::reset_frame();
    runner.tick(dt);
    EntityHelper::merge_entity_arrays();
    handlers.tick_all(EntityHelper::get_entities_for_mod(), dt);
  }

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
      log_warn("e2e: screenshot '{}' failed to write {}", name, path.string());
  }

  bool finished() const { return runner.is_finished(); }

  int finish() const {
    runner.print_results();
    return runner.is_finished() && !runner.has_failed() ? 0 : 1;
  }
};

} // namespace testing
} // namespace afterhours
