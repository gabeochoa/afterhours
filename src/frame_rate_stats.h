#pragma once

#include <algorithm>

#include "logging.h"

namespace afterhours {

struct FrameRateStats {
  explicit FrameRateStats(float window_seconds = 1.f)
      : window(window_seconds) {
    if (window <= 0.f) {
      log_error("FrameRateStats: window must be positive, got {}", window);
      window = 1.f;
    }
  }

  bool add(float dt) {
    if (dt < 0.f) {
      log_warn("FrameRateStats: negative dt {} ignored", dt);
      return false;
    }
    elapsed += static_cast<double>(dt);
    frames++;
    longest = std::max(longest, dt);
    if (elapsed < static_cast<double>(window))
      return false;
    last_fps = static_cast<float>(frames / elapsed);
    last_avg_ms = static_cast<float>(elapsed * 1000.0 / frames);
    last_max_ms = longest * 1000.f;
    elapsed = 0.0;
    frames = 0;
    longest = 0.f;
    has_window = true;
    return true;
  }

  [[nodiscard]] bool ready() const { return has_window; }
  [[nodiscard]] float fps() const { return last_fps; }
  [[nodiscard]] float avg_frame_ms() const { return last_avg_ms; }
  [[nodiscard]] float max_frame_ms() const { return last_max_ms; }

private:
  float window;
  double elapsed = 0.0;
  int frames = 0;
  float longest = 0.f;
  bool has_window = false;
  float last_fps = 0.f;
  float last_avg_ms = 0.f;
  float last_max_ms = 0.f;
};

} // namespace afterhours
