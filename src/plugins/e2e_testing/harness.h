#pragma once

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>

#include "../../graphics_common.h"
#include "runner.h"

namespace afterhours {
namespace testing {

// The library shipped an e2e engine and no wiring, so every consumer wrote the
// same argument parser. Four of them did, and they drifted: kart wants
// --screenshot-dir and --e2e-speed that wm never had, cartographer asked for a
// shared command pack, pong for registration it does not have to repeat.
//
// Only the app-specific part stays in the app: which systems to register, and
// what a screenshot does with its name. Everything here is the same everywhere.
struct E2EArgs {
  bool enabled = false;
  std::string script_path;
  std::string script_dir;
  // Where `screenshot <name>` should write. Empty means the app decides, which
  // is what every app was hardcoding.
  std::string screenshot_dir;
  float timeout_seconds = 30.0f;
  bool slow_mode = false;
  float slow_delay = 0.5f;
  bool update_baselines = false;
  bool headless = false;
  bool quiet = false;
  // Multiplies the host's dt. Note this does NOT change wait_frames, which
  // counts ticks; it changes `wait <seconds>`.
  float time_scale = 1.0f;
  int capture_interval = 0;
  std::string results_json;

  // For RunConfig::display, so a consumer need not map the bool themselves.
  graphics::DisplayMode display_mode() const {
    return headless ? graphics::DisplayMode::Headless
                    : graphics::DisplayMode::Windowed;
  }
};

inline bool should_run_e2e(int argc, char *argv[]) {
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    const std::string key = arg.substr(0, arg.find('='));
    if (key == "--e2e" || key == "--test-mode" || key == "--test-script" ||
        key == "--test-script-dir")
      return true;
  }
  return false;
}

inline E2EArgs parse_e2e_args(int argc, char *argv[]) {
  E2EArgs args;
  const auto next = [&](int &i) -> std::string {
    return (i + 1 < argc) ? argv[++i] : std::string{};
  };

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    const auto eq = arg.find('=');
    const std::string key =
        eq == std::string::npos ? arg : arg.substr(0, eq);
    const auto value = [&]() -> std::string {
      return eq == std::string::npos ? next(i) : arg.substr(eq + 1);
    };
    if (key == "--e2e" || key == "--test-mode") {
      args.enabled = true;
    } else if (key == "--test-script") {
      args.script_path = value();
      args.enabled = true;
    } else if (key == "--test-script-dir") {
      args.script_dir = value();
      args.enabled = true;
    } else if (key == "--screenshot-dir") {
      args.screenshot_dir = value();
    } else if (key == "--timeout" || key == "--e2e-timeout") {
      args.timeout_seconds = std::stof(value());
    } else if (key == "--slow") {
      args.slow_mode = true;
    } else if (key == "--slow-delay") {
      args.slow_delay = std::stof(value());
      args.slow_mode = true;
    } else if (key == "--update-baselines") {
      args.update_baselines = true;
    } else if (key == "--headless") {
      args.headless = true;
    } else if (key == "--quiet") {
      args.quiet = true;
      // --e2e-speed is kart's spelling of the same knob. Both accepted rather
      // than making one project rename its scripts.
    } else if (arg == "--time-scale" || arg == "--e2e-speed") {
      args.time_scale = std::stof(next(i));
    } else if (arg == "--capture-interval") {
      args.capture_interval = std::stoi(next(i));
    } else if (arg == "--results-json") {
      args.results_json = next(i);
    } else if (key == "--time-scale" || key == "--e2e-speed") {
      args.time_scale = std::stof(value());
    } else if (key == "--capture-interval") {
      args.capture_interval = std::stoi(value());
    }
  }
  return args;
}

// Resolve where `screenshot <name>` writes. Falls back to the app's own
// default when --screenshot-dir was not given.
inline std::string screenshot_path(const E2EArgs &args, const std::string &name,
                                   const std::string &fallback_dir) {
  const std::string dir = args.screenshot_dir.empty() ? fallback_dir
                                                      : args.screenshot_dir;
  if (dir.empty())
    return name + ".png";
  return (std::filesystem::path(dir) / (name + ".png")).string();
}

inline int count_e2e_scripts(const std::string &dir) {
  if (dir.empty() || !std::filesystem::is_directory(dir))
    return 0;
  int count = 0;
  for (const auto &entry : std::filesystem::directory_iterator(dir))
    if (entry.path().extension() == ".e2e")
      ++count;
  return count;
}

// Reference wiring for `--time-scale` on a fixed-step sim: speed comes from
// more fixed steps, never a bigger dt. Scaling dt (headless get_delta_time,
// motion::scaled_dt) changes behaviour, and setting fixed_dt alone makes
// --time-scale a no-op because RunConfig::step_dt ignores the scale for a
// fixed step — so the flag and the sim never met until this.
inline int substeps_for_time_scale(float time_scale) {
  // Fractional scales round to a whole step; below 1 can't mean fewer than
  // one step, so slow the frame down instead of dropping steps.
  return std::max(1, static_cast<int>(std::lround(time_scale)));
}

// Turns the flag into substeps on a RunConfig with fixed_dt set; a no-op
// (one step of wall clock) without it, where scaling dt is the honest mode.
inline int apply_time_scale(graphics::RunConfig &cfg, float time_scale) {
  if (cfg.fixed_dt > 0.f)
    cfg.sim_steps = std::max(1, cfg.sim_steps) * substeps_for_time_scale(time_scale);
  return cfg.step_count();
}

// One frame of the reference loop: step_count() fixed ticks of fixed_dt, so
// --time-scale 4 is 4 ticks of 1/60, counted, not one tick of 4/60.
inline void run_fixed_steps(SystemManager &sm, Entities &entities,
                            const graphics::RunConfig &cfg, float wall_dt) {
  const float dt = cfg.step_dt(wall_dt);
  for (int i = 0; i < cfg.step_count(); ++i)
    sm.fixed_tick(entities, dt);
}

inline void configure_runner(E2ERunner &runner, const E2EArgs &args) {
  runner.set_timeout(args.timeout_seconds);
  if (args.slow_mode)
    runner.set_slow_mode(true, args.slow_delay);
  if (!args.results_json.empty())
    runner.set_results_path(args.results_json);
}

} // namespace testing
} // namespace afterhours
