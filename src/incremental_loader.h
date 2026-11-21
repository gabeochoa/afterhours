#pragma once

#include <cstddef>
#include <deque>
#include <functional>

namespace afterhours {

// Main-thread incremental loader: queue load steps (e.g. one
// ModelLibrary::load per step) and pump a slice each frame so the loading
// screen stays a normal system. Distinct from jobs::Pool (draft #78):
// steps run on the caller's thread, never on workers.
struct IncrementalLoader {
  using Step = std::function<void()>;
  using ClockFn = std::function<double()>;  // monotonic milliseconds

  void add(Step step) { steps_.push_back(std::move(step)); }
  void clear() { steps_.clear(); done_ = 0; cancelled_ = false; }

  // Drops pending steps; work already run is not undone.
  void cancel() { steps_.clear(); cancelled_ = true; }
  [[nodiscard]] bool cancelled() const { return cancelled_; }

  [[nodiscard]] size_t total() const { return done_ + steps_.size(); }
  [[nodiscard]] size_t completed() const { return done_; }
  [[nodiscard]] size_t remaining() const { return steps_.size(); }
  [[nodiscard]] bool done() const { return steps_.empty(); }
  [[nodiscard]] float progress() const {
    return total() == 0 ? 1.f
                        : static_cast<float>(done_) / static_cast<float>(total());
  }

  size_t pump_items(size_t max_items) {
    size_t ran = 0;
    while (ran < max_items && !steps_.empty()) run_one(), ++ran;
    return ran;
  }

  // Runs at least one step (a step may exceed the budget; stalling forever
  // helps no one), then stops once the injected clock passes the budget.
  size_t pump_time(double budget_ms, const ClockFn &now_ms) {
    if (steps_.empty() || !now_ms) return 0;
    const double start = now_ms();
    size_t ran = 0;
    do {
      run_one();
      ++ran;
    } while (!steps_.empty() && now_ms() - start < budget_ms);
    return ran;
  }

 private:
  void run_one() {
    Step step = std::move(steps_.front());
    steps_.pop_front();
    ++done_;  // count before running so progress is sane if a step re-enters
    if (step) step();
  }

  std::deque<Step> steps_;
  size_t done_ = 0;
  bool cancelled_ = false;
};

}  // namespace afterhours
