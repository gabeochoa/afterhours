// E2E Testing Framework - Screen-level pixel assertions
//
// expect_screenshot needs baselines: something must be captured once honestly
// before the diff means anything. expect_screen_luma catches the regression
// without one -- a black screen, a washed-out screen, a missing background --
// by asserting the average luminance of the whole frame lands in a range.
#pragma once

#include <chrono>
#include <cmath>
#include <format>
#include <functional>
#include <optional>
#include <string>

#include "../../image.h"
#include "../../logging.h"
#include "../../warn_once.h"
#include "../color.h"
#include "pending_command.h"

#if defined(AFTER_HOURS_USE_RAYLIB) || defined(AFTER_HOURS_SOKOL)
#include "../../graphics.h"
#include <filesystem>
#endif

namespace afterhours {
namespace testing {
namespace screen_commands {

// Mean relative luminance (WCAG, 0..1) over every pixel. Empty image is
// nullopt so a failed readback cannot look like a black screen.
inline std::optional<float> mean_luma(const Image &image) {
  if (image.width <= 0 || image.height <= 0 || image.rgba.empty())
    return std::nullopt;
  double sum = 0.0;
  const std::size_t count =
      static_cast<std::size_t>(image.width) *
      static_cast<std::size_t>(image.height);
  for (std::size_t i = 0; i < count; i++) {
    const Color c{image.rgba[i * 4], image.rgba[i * 4 + 1],
                  image.rgba[i * 4 + 2], image.rgba[i * 4 + 3]};
    sum += colors::luminance(c);
  }
  return static_cast<float>(sum / static_cast<double>(count));
}

// The readback half is the consumer's: a provider returning the mean luma of
// the current frame. Tests set a constant; apps set builtin_provider().
struct LumaProvider {
  std::function<std::optional<float>()> get_mean_luma;
};

inline LumaProvider &provider() {
  static LumaProvider p;
  return p;
}

inline void set_provider(LumaProvider p) {
  if (provider().get_mean_luma)
    warn_once(0, "screen_commands::set_provider: overwriting an existing "
                 "luma provider, the previous provider is discarded");
  provider() = std::move(p);
}

#if defined(AFTER_HOURS_USE_RAYLIB) || defined(AFTER_HOURS_SOKOL)
// capture_frame-style readback composed from main: PNG to a temp file,
// decoded with the CPU image API (#81) in src/image.h.
inline void set_builtin_provider() {
  auto p = LumaProvider{};
  p.get_mean_luma = []() -> std::optional<float> {
    const auto path =
        std::filesystem::temp_directory_path() /
        ("afterhours-luma-" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()) +
         ".png");
    if (!graphics::capture_frame(path))
      return std::nullopt;
    auto image = load_image(path);
    std::filesystem::remove(path);
    if (!image)
      return std::nullopt;
    return mean_luma(*image);
  };
  set_provider(std::move(p));
}
#endif

// expect_screen_luma <min> <max> (0..1). Retries while out of range, like
// other expect_* assertions, so a frame that lands a tick late still passes.
struct HandleExpectScreenLumaCommand : System<PendingE2ECommand> {
  void for_each_with(Entity &, PendingE2ECommand &cmd, float) override {
    if (cmd.is_consumed() || !cmd.is("expect_screen_luma"))
      return;
    if (!cmd.has_args(2) || !cmd.maybe_arg_as<float>(0) ||
        !cmd.maybe_arg_as<float>(1)) {
      cmd.fail("expect_screen_luma requires: <min> <max> (0..1)");
      return;
    }
    const float min_luma = cmd.arg_as<float>(0);
    const float max_luma = cmd.arg_as<float>(1);
    if (!(min_luma <= max_luma) || min_luma < 0.f || max_luma > 1.f ||
        !std::isfinite(min_luma) || !std::isfinite(max_luma)) {
      cmd.fail(std::format("expect_screen_luma: invalid range [{}, {}]",
                           min_luma, max_luma));
      return;
    }

    LumaProvider &p = provider();
    if (!p.get_mean_luma) {
      cmd.fail("expect_screen_luma unavailable: no luma provider configured");
      return;
    }
    std::optional<float> luma = p.get_mean_luma();
    if (!luma) {
      cmd.retry_with("expect_screen_luma: frame readback unavailable");
      return;
    }
    cmd.expect(*luma >= min_luma && *luma <= max_luma,
               std::format("expect_screen_luma: measured {:.3f}", *luma));
    if (cmd.is_consumed() && cmd.succeeded())
      log_info("expect_screen_luma: {:.3f} in [{:.3f}, {:.3f}] (PASS)", *luma,
               min_luma, max_luma);
  }
};

inline void register_screen_commands(SystemManager &sm) {
  sm.register_update_system(
      std::make_unique<HandleExpectScreenLumaCommand>());
}

}  // namespace screen_commands
}  // namespace testing
}  // namespace afterhours
