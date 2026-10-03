#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>

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

} // namespace afterhours::frame_loop
