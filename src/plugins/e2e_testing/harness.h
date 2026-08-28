#pragma once

#include <filesystem>
#include <string>

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
};

inline bool should_run_e2e(int argc, char *argv[]) {
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--e2e" || arg == "--test-mode" || arg == "--test-script" ||
        arg == "--test-script-dir")
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
    if (arg == "--e2e" || arg == "--test-mode") {
      args.enabled = true;
    } else if (arg == "--test-script") {
      args.script_path = next(i);
      args.enabled = true;
    } else if (arg == "--test-script-dir") {
      args.script_dir = next(i);
      args.enabled = true;
    } else if (arg == "--screenshot-dir") {
      args.screenshot_dir = next(i);
    } else if (arg == "--timeout") {
      args.timeout_seconds = std::stof(next(i));
    } else if (arg == "--slow") {
      args.slow_mode = true;
    } else if (arg == "--slow-delay") {
      args.slow_delay = std::stof(next(i));
      args.slow_mode = true;
    } else if (arg == "--update-baselines") {
      args.update_baselines = true;
    } else if (arg == "--headless") {
      args.headless = true;
    } else if (arg == "--quiet") {
      args.quiet = true;
      // --e2e-speed is kart's spelling of the same knob. Both accepted rather
      // than making one project rename its scripts.
    } else if (arg == "--time-scale" || arg == "--e2e-speed") {
      args.time_scale = std::stof(next(i));
    } else if (arg == "--capture-interval") {
      args.capture_interval = std::stoi(next(i));
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

inline void configure_runner(E2ERunner &runner, const E2EArgs &args) {
  runner.set_timeout(args.timeout_seconds);
  if (args.slow_mode)
    runner.set_slow_mode(true, args.slow_delay);
}

} // namespace testing
} // namespace afterhours
