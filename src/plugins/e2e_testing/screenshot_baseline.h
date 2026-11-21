#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>

#include "../../image.h"
#include "pending_command.h"

namespace afterhours {
namespace testing {

struct ScreenshotBaselineOptions {
  int channel_tolerance = 8;
  double max_mismatch_pct = 0.05;
  std::string baseline_dir = "screenshot-baselines";
  bool update_baselines = false;
};

struct ScreenshotDiff {
  bool size_match = false;
  long long mismatched_pixels = 0;
  long long total_pixels = 0;
  int max_channel_delta = 0;
  double mismatch_pct() const {
    return total_pixels == 0 ? 100.0
                             : 100.0 * static_cast<double>(mismatched_pixels) /
                                   static_cast<double>(total_pixels);
  }
  bool within(const ScreenshotBaselineOptions &o) const {
    return size_match && mismatch_pct() <= o.max_mismatch_pct;
  }
};

inline ScreenshotDiff diff_images(const Image &a, const Image &b,
                                  int channel_tolerance = 8) {
  ScreenshotDiff d;
  d.total_pixels = static_cast<long long>(a.width) * a.height;
  if (a.width != b.width || a.height != b.height) return d;
  d.size_match = true;
  const std::size_t n =
      std::min(a.rgba.size(), b.rgba.size()) / 4;
  for (std::size_t i = 0; i < n; ++i) {
    int mx = 0;
    for (int c = 0; c < 4; ++c) {
      int delta = std::abs(static_cast<int>(a.rgba[i * 4 + c]) -
                           static_cast<int>(b.rgba[i * 4 + c]));
      mx = std::max(mx, delta);
    }
    d.max_channel_delta = std::max(d.max_channel_delta, mx);
    if (mx > channel_tolerance) ++d.mismatched_pixels;
  }
  return d;
}

enum class ExpectScreenshotStatus {
  Passed,
  Updated,
  MissingBaseline,
  Mismatch,
  SizeMismatch,
  UnreadableCurrent,
  UnreadableBaseline,
};

struct ExpectScreenshotResult {
  ExpectScreenshotStatus status;
  ScreenshotDiff diff;
  std::string baseline_path;
  bool passed() const {
    return status == ExpectScreenshotStatus::Passed ||
           status == ExpectScreenshotStatus::Updated;
  }
};

inline ScreenshotBaselineOptions
screenshot_baseline_options(bool update_baselines,
                            const std::string &screenshot_dir = "") {
  ScreenshotBaselineOptions opts;
  opts.update_baselines = update_baselines;
  if (!screenshot_dir.empty()) opts.baseline_dir = screenshot_dir;
  return opts;
}

inline std::string baseline_path_for(const ScreenshotBaselineOptions &o,
                                     const std::string &name) {
  return (std::filesystem::path(o.baseline_dir) / (name + ".png")).string();
}

inline ExpectScreenshotResult
check_screenshot_baseline(const std::filesystem::path &current_path,
                          const std::string &name,
                          const ScreenshotBaselineOptions &opts) {
  const auto baseline = baseline_path_for(opts, name);
  ExpectScreenshotResult r{ExpectScreenshotStatus::Mismatch, {}, baseline};
  if (opts.update_baselines) {
    std::error_code ec;
    std::filesystem::create_directories(opts.baseline_dir, ec);
    std::filesystem::copy_file(
        current_path, baseline,
        std::filesystem::copy_options::overwrite_existing, ec);
    r.status = ec ? ExpectScreenshotStatus::UnreadableCurrent
                  : ExpectScreenshotStatus::Updated;
    return r;
  }
  if (!std::filesystem::exists(baseline)) {
    r.status = ExpectScreenshotStatus::MissingBaseline;
    return r;
  }
  auto current = load_image(current_path);
  if (!current) {
    r.status = ExpectScreenshotStatus::UnreadableCurrent;
    return r;
  }
  auto base = load_image(baseline);
  if (!base) {
    r.status = ExpectScreenshotStatus::UnreadableBaseline;
    return r;
  }
  r.diff = diff_images(*current, *base, opts.channel_tolerance);
  if (!r.diff.size_match)
    r.status = ExpectScreenshotStatus::SizeMismatch;
  else
    r.status = r.diff.within(opts) ? ExpectScreenshotStatus::Passed
                                   : ExpectScreenshotStatus::Mismatch;
  return r;
}

inline std::string
expect_screenshot_message(const std::string &name,
                          const ExpectScreenshotResult &r,
                          const ScreenshotBaselineOptions &opts) {
  switch (r.status) {
    case ExpectScreenshotStatus::Passed:
      return "";
    case ExpectScreenshotStatus::Updated:
      return "";
    case ExpectScreenshotStatus::MissingBaseline:
      return "expect_screenshot '" + name + "': missing baseline " +
             r.baseline_path + " (run with --update-baselines to create it)";
    case ExpectScreenshotStatus::UnreadableCurrent:
      return "expect_screenshot '" + name + "': captured image unreadable";
    case ExpectScreenshotStatus::UnreadableBaseline:
      return "expect_screenshot '" + name + "': baseline unreadable: " +
             r.baseline_path;
    case ExpectScreenshotStatus::SizeMismatch:
      return "expect_screenshot '" + name + "': size mismatch vs " +
             r.baseline_path;
    case ExpectScreenshotStatus::Mismatch:
      return "expect_screenshot '" + name + "': " +
             std::to_string(r.diff.mismatched_pixels) + "/" +
             std::to_string(r.diff.total_pixels) + " pixels differ (" +
             std::to_string(r.diff.mismatch_pct()) + "%, budget " +
             std::to_string(opts.max_mismatch_pct) + "%, max channel delta " +
             std::to_string(r.diff.max_channel_delta) + ") vs " +
             r.baseline_path;
  }
  return "expect_screenshot '" + name + "': failed";
}

// Capture-to-path callback: writes the current frame as PNG to `path`.
using ScreenshotCaptureFn = std::function<bool(const std::string &)>;

inline void take_expect_screenshot(const ScreenshotCaptureFn &fn,
                                   PendingE2ECommand &cmd,
                                   const ScreenshotBaselineOptions &opts) {
    if (!cmd.has_args(1)) {
        cmd.fail("expect_screenshot requires a baseline name");
        return;
    }
    if (!fn) {
        cmd.fail("expect_screenshot capture callback not set");
        return;
    }
    const std::string &name = cmd.args[0];
    if (opts.update_baselines) {
        std::error_code ec;
        std::filesystem::create_directories(opts.baseline_dir, ec);
        if (!fn(baseline_path_for(opts, name))) {
            cmd.fail(std::format("expect_screenshot '{}' failed to capture", name));
            return;
        }
        cmd.consume();
        return;
    }
    const auto current = std::filesystem::temp_directory_path() /
                           ("afterhours-expect-" + name + ".png");
    if (!fn(current.string())) {
        cmd.fail(std::format("expect_screenshot '{}' failed to capture", name));
        return;
    }
    auto result = check_screenshot_baseline(current, name, opts);
    std::error_code ec;
    std::filesystem::remove(current, ec);
    if (result.passed())
        cmd.consume();
    else
        cmd.fail(expect_screenshot_message(name, result, opts));
}

struct HandleExpectScreenshotCommand : System<PendingE2ECommand> {
    HandleExpectScreenshotCommand(ScreenshotCaptureFn fn,
                                  ScreenshotBaselineOptions opts = {})
        : capture_fn_(std::move(fn)), opts_(std::move(opts)) {}

    virtual void for_each_with(Entity &, PendingE2ECommand &cmd,
                               float) override {
        if (cmd.is_consumed() || !cmd.is("expect_screenshot")) return;
        take_expect_screenshot(capture_fn_, cmd, opts_);
    }

   private:
    ScreenshotCaptureFn capture_fn_;
    ScreenshotBaselineOptions opts_;
};


} // namespace testing
} // namespace afterhours
