#pragma once

// Shared scaffolding for afterhours examples.
//
// The core section is always available: Checks + fixed-step system running.
// Define AFTER_HOURS_EXAMPLE_UI before including for the imm-UI harnesses,
// AFTER_HOURS_EXAMPLE_GRAPHICS for graphics RAII / headless capture, and
// AFTER_HOURS_EXAMPLE_TEST_INPUT for synthetic clicks in headless UI tests.
// Define AFTER_HOURS_EXAMPLE_E2E for the script-runner harness.
// Examples should only opt into the sections they actually demonstrate.

#include <cmath>
#include <cstdio>
#include <memory>
#include <utility>

#include "../ah.h"

#if defined(AFTER_HOURS_EXAMPLE_UI) && !defined(AFTER_HOURS_IMM_UI)
#define AFTER_HOURS_IMM_UI
#endif

#ifdef AFTER_HOURS_EXAMPLE_GRAPHICS
#include "../src/graphics.h"
#endif

#ifdef AFTER_HOURS_EXAMPLE_UI
#include "../src/plugins/ui.h"
#endif

#ifdef AFTER_HOURS_EXAMPLE_TEST_INPUT
#include "../src/plugins/e2e_testing/platform_test_input.h"
#endif

#ifdef AFTER_HOURS_EXAMPLE_E2E
#include <functional>
#include <set>
#include <string>

#include "../src/plugins/e2e_testing/e2e_testing.h"
#include "../src/plugins/e2e_testing/harness.h"
#endif

namespace afterhours::examples {

struct Checks {
  int run_count = 0;
  int failed_count = 0;

  void check(bool condition, const char *what) {
    ++run_count;
    if (condition) return;
    ++failed_count;
    std::printf("  FAIL: %s\n", what);
  }

  void near(float actual, float expected, float tolerance, const char *what) {
    ++run_count;
    if (std::fabs(actual - expected) <= tolerance) return;
    ++failed_count;
    std::printf("  FAIL: %s (got %.4f, expected %.4f +/- %.4f)\n", what,
                actual, expected, tolerance);
  }

  int finish(const char *name) const {
    std::printf("%s: %d/%d checks passed\n", name, run_count - failed_count,
                run_count);
    return failed_count == 0 ? 0 : 1;
  }
};

inline void run_frames(SystemManager &systems, int frames,
                       float dt = 1.f / 60.f) {
  for (int frame = 0; frame < frames; ++frame) systems.run(dt);
}

inline void run_seconds(SystemManager &systems, float seconds,
                        float dt = 1.f / 60.f) {
  for (float t = 0.f; t < seconds - 1e-6f; t += dt) systems.run(dt);
}

template <typename... Systems>
SystemManager update_systems() {
  SystemManager systems;
  (systems.register_update_system(std::make_unique<Systems>()), ...);
  return systems;
}

template <typename... Systems>
void add_update_systems(SystemManager &systems, Systems &&...user_systems) {
  (systems.register_update_system(std::forward<Systems>(user_systems)), ...);
}

#ifdef AFTER_HOURS_EXAMPLE_UI

enum struct InputAction {
  None,
  WidgetMod,
  WidgetNext,
  WidgetBack,
  WidgetPress,
  WidgetLeft,
  WidgetRight,
  WidgetUp,
  WidgetDown,
  MenuBack,
  TextBackspace,
  TextCopy,
  TextCut,
  TextDelete,
  TextDeleteWordBack,
  TextDeleteWordForward,
  TextEnd,
  TextHome,
  TextPaste,
  TextRedo,
  TextSelectAll,
  TextSelectLeft,
  TextSelectRight,
  TextUndo,
  TextWordLeft,
  TextWordRight,
};

using ExampleUIContext = ui::UIContext<InputAction>;

template <typename Screen, typename Action = ui::DefaultAction> struct LogicUI {
  SystemManager systems;
  Screen &screen;

  template <typename... ExtraSystems>
  LogicUI(window_manager::Resolution resolution, std::unique_ptr<Screen> s,
          ExtraSystems &&...extra_systems)
      : screen(*s) {
    ui::setup_with_resolution<Action>(
        systems, resolution, std::move(s),
        std::forward<ExtraSystems>(extra_systems)...);
  }

  void run(int frames = 1, float dt = 1.f / 60.f) {
    run_frames(systems, frames, dt);
  }

  ui::UIContext<Action> &context() {
    return *EntityHelper::get_singleton_cmp<ui::UIContext<Action>>();
  }
};

template <typename Screen, typename Action = ui::DefaultAction,
          typename... ExtraSystems>
LogicUI<Screen, Action> make_logic_ui(window_manager::Resolution resolution,
                                      std::unique_ptr<Screen> screen,
                                      ExtraSystems &&...extra_systems) {
  return LogicUI<Screen, Action>(
      resolution, std::move(screen),
      std::forward<ExtraSystems>(extra_systems)...);
}

#endif // AFTER_HOURS_EXAMPLE_UI

#ifdef AFTER_HOURS_EXAMPLE_GRAPHICS

struct Graphics {
  bool initialized = false;

  explicit Graphics(const graphics::Config &config)
      : initialized(graphics::init(config)) {}

  ~Graphics() {
    if (initialized) graphics::shutdown();
  }

  Graphics(const Graphics &) = delete;
  Graphics &operator=(const Graphics &) = delete;
  Graphics(Graphics &&) = delete;
  Graphics &operator=(Graphics &&) = delete;

  bool ok() const { return initialized; }

  static graphics::Config headless(int width, int height,
                                   const char *title = "example") {
    graphics::Config config{};
    config.display = graphics::DisplayMode::Headless;
    config.width = width;
    config.height = height;
    config.title = title;
    return config;
  }

  static graphics::Config hidden_window(int width, int height,
                                        const char *title = "example") {
    graphics::Config config{};
    config.display = graphics::DisplayMode::Windowed;
    config.width = width;
    config.height = height;
    config.title = title;
    config.enable_msaa = false;
#ifdef AFTER_HOURS_USE_RAYLIB
    config.config_flags = raylib::FLAG_WINDOW_HIDDEN;
#endif
    return config;
  }
};

#ifdef AFTER_HOURS_EXAMPLE_UI

template <typename Screen, typename Action = ui::DefaultAction> struct HeadlessUI {
  Graphics graphics;
  LogicUI<Screen, Action> ui;

  template <typename... ExtraSystems>
  HeadlessUI(int width, int height, std::unique_ptr<Screen> screen,
             ExtraSystems &&...extra_systems)
      : graphics(Graphics::headless(width, height)),
        ui(window_manager::Resolution{width, height}, std::move(screen),
           std::forward<ExtraSystems>(extra_systems)...) {
#ifdef AFTER_HOURS_EXAMPLE_TEST_INPUT
    testing::platform_input::set_test_mode(true);
#endif
  }

  bool ok() const { return graphics.ok(); }
  Screen &screen() { return ui.screen; }

  void run(int frames = 1, float dt = 1.f / 60.f) { ui.run(frames, dt); }

#ifdef AFTER_HOURS_EXAMPLE_TEST_INPUT
  void click_at(float x, float y) {
    testing::test_input::simulate_click(x, y);
    for (int frame = 0; frame < 3; ++frame) {
      ui.run(1);
      testing::test_input::reset_frame();
    }
  }
#endif
};

template <typename Action = InputAction> struct ImmScreenshot {
  Graphics graphics;
  SystemManager systems;

  template <typename... UserSystems>
  ImmScreenshot(int width, int height, bool batched, UserSystems &&...user_systems)
      : graphics(Graphics::hidden_window(width, height)) {
    if (!graphics.ok()) return;

    Entity &ui_root = ui::init_ui_plugin<Action>();
    ui_root.addComponent<window_manager::ProvidesCurrentResolution>(
        window_manager::Resolution{width, height});
    EntityHelper::registerSingleton<window_manager::ProvidesCurrentResolution>(
        ui_root);
    ui::UICollectionHolder::get()
        .collection
        .registerSingleton<window_manager::ProvidesCurrentResolution>(ui_root);

    if (!EntityHelper::has_singleton<input::InputCollector>()) {
      Entity &input_root = EntityHelper::createEntity();
      input_root.addComponent<input::InputCollector>();
      EntityHelper::registerSingleton<input::InputCollector>(input_root);
    }

    ui::enforce_singletons<Action>(systems);
    ui::register_before_ui_updates<Action>(systems);
    add_update_systems(systems, std::forward<UserSystems>(user_systems)...);
    ui::register_after_ui_updates<Action>(systems);
    if (batched)
      ui::register_batched_render_systems<Action>(systems);
    else
      ui::register_render_systems<Action>(systems);
  }

  bool ok() const { return graphics.ok(); }

  bool capture(const std::filesystem::path &path, Color clear) {
    if (!ok()) return false;
    graphics::begin_frame();
    graphics::clear_background(clear);
    systems.run(1.f / 60.f);
    graphics::end_frame();
    return graphics::capture_frame(path);
  }
};

#endif // AFTER_HOURS_EXAMPLE_UI
#endif // AFTER_HOURS_EXAMPLE_GRAPHICS

#ifdef AFTER_HOURS_EXAMPLE_E2E

struct E2EHarness {
  testing::E2EArgs args;
  SystemManager systems;
  testing::E2ERunner runner;
  bool handlers_finished = false;

  E2EHarness(int argc, char **argv)
      : args(testing::parse_e2e_args(argc, argv)) {
    testing::register_builtin_handlers(systems);
    testing::configure_runner(runner, args);
  }

  E2EHarness(const E2EHarness &) = delete;
  E2EHarness &operator=(const E2EHarness &) = delete;

  template <typename... Systems> void add_systems(Systems &&...user_systems) {
    add_update_systems(systems, std::forward<Systems>(user_systems)...);
  }

  bool has_script() const {
    return !args.script_path.empty() || !args.script_dir.empty();
  }

  void finish_handlers(std::set<std::string> handled_elsewhere = {}) {
    if (handlers_finished) return;
    if (handled_elsewhere.empty())
      testing::register_unknown_handler(systems);
    else
      testing::register_unknown_handler(systems, std::move(handled_elsewhere));
    testing::register_cleanup(systems);
    handlers_finished = true;
  }

  int run(const std::function<void()> &per_frame = nullptr,
          float dt = 1.f / 60.f) {
    finish_handlers();
    if (!args.script_dir.empty())
      runner.load_scripts_from_directory(args.script_dir);
    else
      runner.load_script(args.script_path);

    while (!runner.is_finished()) {
      runner.tick(dt);
      if (per_frame) per_frame();
      systems.run(dt);
    }
    runner.print_results();
    return runner.has_failed() ? 1 : 0;
  }
};

#endif // AFTER_HOURS_EXAMPLE_E2E

} // namespace afterhours::examples
