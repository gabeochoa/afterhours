#pragma once

#include <cstddef>
#include <deque>
#include <functional>

namespace afterhours {

struct IncrementalLoader {
  using Step = std::function<void()>;
  using ClockFn = std::function<double()>;

  void add(Step step) { steps.push_back(std::move(step)); }
  void clear() { steps.clear(); done_count = 0; is_cancelled = false; }

  void cancel() { steps.clear(); is_cancelled = true; }
  [[nodiscard]] bool cancelled() const { return is_cancelled; }

  [[nodiscard]] size_t total() const { return done_count + steps.size(); }
  [[nodiscard]] size_t completed() const { return done_count; }
  [[nodiscard]] size_t remaining() const { return steps.size(); }
  [[nodiscard]] bool done() const { return steps.empty(); }
  [[nodiscard]] float progress() const {
    return total() == 0 ? 1.f
                        : static_cast<float>(done_count) / static_cast<float>(total());
  }

  size_t pump_items(size_t max_items) {
    size_t ran = 0;
    while (ran < max_items && !steps.empty()) run_one(), ++ran;
    return ran;
  }

  size_t pump_time(double budget_ms, const ClockFn &now_ms) {
    if (steps.empty() || !now_ms) return 0;
    const double start = now_ms();
    size_t ran = 0;
    do {
      run_one();
      ++ran;
    } while (!steps.empty() && now_ms() - start < budget_ms);
    return ran;
  }

 private:
  void run_one() {
    Step step = std::move(steps.front());
    steps.pop_front();
    ++done_count;
    if (step) step();
  }

  std::deque<Step> steps;
  size_t done_count = 0;
  bool is_cancelled = false;
};

}
