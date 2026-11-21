#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <span>

namespace afterhours::frame_loop {

enum class Mode { Continuous, OnDemand };

struct Stats {
  uint64_t requests = 0;
  uint64_t frames_run = 0;
  uint64_t frames_skipped = 0;
};

namespace detail {
struct State {
  std::mutex mutex;
  std::condition_variable cv;
  Mode mode = Mode::Continuous;
  bool requested = false;
  Stats stats;
};
inline State &state() {
  static State s;
  return s;
}
} // namespace detail

inline void set_mode(Mode mode) {
  auto &s = detail::state();
  {
    std::lock_guard lock(s.mutex);
    s.mode = mode;
  }
  s.cv.notify_all();
}

inline Mode mode() {
  auto &s = detail::state();
  std::lock_guard lock(s.mutex);
  return s.mode;
}

inline void request_frame() {
  auto &s = detail::state();
  {
    std::lock_guard lock(s.mutex);
    s.requested = true;
    s.stats.requests++;
  }
  s.cv.notify_all();
}

inline bool frame_requested() {
  auto &s = detail::state();
  std::lock_guard lock(s.mutex);
  return s.requested;
}

inline bool consume_frame_request() {
  auto &s = detail::state();
  std::lock_guard lock(s.mutex);
  const bool granted = s.requested;
  s.requested = false;
  return granted;
}

inline bool wait_for_frame_request(int timeout_ms) {
  auto &s = detail::state();
  std::unique_lock lock(s.mutex);
  auto ready = [&s] { return s.requested || s.mode == Mode::Continuous; };
  s.cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), ready);
  return ready();
}

inline void note_frame(bool ran) {
  auto &s = detail::state();
  std::lock_guard lock(s.mutex);
  (ran ? s.stats.frames_run : s.stats.frames_skipped)++;
}

inline Stats stats() {
  auto &s = detail::state();
  std::lock_guard lock(s.mutex);
  return s.stats;
}

inline void reset_stats() {
  auto &s = detail::state();
  std::lock_guard lock(s.mutex);
  s.stats = {};
  s.requested = false;
}

struct CadenceDecision {
  bool admitted = false;
  double dt = 0.0; // seconds since the previous admitted frame
};

struct CadenceStats {
  uint64_t callbacks = 0;
  uint64_t admitted = 0;
  uint64_t skipped = 0;
};

// Pure admission decision extracted from the sokol frame callback so it
// can be driven headlessly with synthetic callback times.
class CadenceDriver {
public:
  CadenceDriver() = default;
  CadenceDriver(Mode mode, int target_fps) { configure(mode, target_fps); }

  void configure(Mode mode, int target_fps) {
    mode_ = mode;
    target_fps_ = target_fps;
    elapsed_ = 0.0;
    stats_ = {};
  }

  // Production seeds elapsed at one period so the first callback can admit.
  void prime() {
    if (period() > 0.0)
      elapsed_ = period();
  }

  // Sokol passes its live globals per callback (its tests set them directly).
  CadenceDecision on_callback(double callback_dt, bool requested, Mode mode,
                              int target_fps) {
    mode_ = mode;
    target_fps_ = target_fps;
    return on_callback(callback_dt, requested);
  }

  CadenceDecision on_callback(double callback_dt, bool requested = false) {
    stats_.callbacks++;
    if (callback_dt > 0.0)
      elapsed_ += callback_dt;
    const bool admitted =
        mode_ == Mode::OnDemand
            ? requested
            : period() <= 0.0 || elapsed_ >= period() - 1e-12;
    if (!admitted) {
      stats_.skipped++;
      return {};
    }
    stats_.admitted++;
    const double dt = elapsed_;
    elapsed_ = 0.0;
    return {true, dt};
  }

  [[nodiscard]] CadenceStats cadence_stats() const { return stats_; }

private:
  [[nodiscard]] double period() const {
    return target_fps_ > 0 ? 1.0 / static_cast<double>(target_fps_) : 0.0;
  }

  Mode mode_ = Mode::Continuous;
  int target_fps_ = 0;
  double elapsed_ = 0.0;
  CadenceStats stats_;
};

template <typename FrameFn>
inline CadenceStats drive_headless_cadence(CadenceDriver &driver,
                                           std::span<const double> callback_dts,
                                           std::span<const bool> requested,
                                           FrameFn &&frame) {
  for (size_t i = 0; i < callback_dts.size(); i++) {
    const auto decision = driver.on_callback(
        callback_dts[i], i < requested.size() ? requested[i] : false);
    if (decision.admitted)
      frame(static_cast<float>(decision.dt));
  }
  return driver.cadence_stats();
}

} // namespace afterhours::frame_loop
