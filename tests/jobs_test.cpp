// jobs_test.cpp
// afterhours::jobs: explicit outcomes, bounded queues, foreground
// reservation, cancellation, shutdown draining must-complete work, tickets,
// and the delayed busy indicator.

#include <afterhours/src/plugins/jobs.h>

#include <atomic>
#include <cstdio>
#include <future>
#include <stdexcept>
#include <string>

using namespace afterhours::jobs;
using namespace std::chrono_literals;

static int checks_run = 0;
static int checks_passed = 0;

static void check(bool cond, const std::string &what) {
  checks_run++;
  if (cond) {
    checks_passed++;
  } else {
    fprintf(stderr, "  FAIL: %s\n", what.c_str());
  }
}

// A job that blocks until the gate opens, signalling when it has started.
struct Gate {
  std::promise<void> open_promise;
  std::shared_future<void> opened = open_promise.get_future().share();
  void open() { open_promise.set_value(); }
};

static void completes_and_fails() {
  Executor pool({.threads = 2});
  auto ok = pool.submit([](std::stop_token) { return 7; });
  check(ok.accepted(), "submission is accepted");
  check(ok.wait_for(2s) && ok.status() == Status::Completed, "job completes");
  check(ok.take() == 7, "take returns the value");
  check(!ok.take().has_value(), "take moves the value out once");
  auto bad = pool.submit([](std::stop_token) -> int { throw std::runtime_error("boom"); });
  check(bad.wait_for(2s) && bad.status() == Status::Failed, "throwing job fails");
  bool message = false;
  try {
    std::rethrow_exception(bad.error());
  } catch (const std::runtime_error &e) {
    message = std::string(e.what()) == "boom";
  }
  check(message, "failure keeps the exception");
  check(!bad.take().has_value(), "failed job has no value");
}

static void foreground_has_a_reserved_worker() {
  Executor pool({.threads = 2, .queue_capacity = 8, .background_queue_capacity = 6});
  Gate gate;
  std::promise<void> entered;
  auto first = pool.submit([&entered, opened = gate.opened](std::stop_token) {
    entered.set_value();
    opened.wait();
    return 1;
  }, {.priority = Priority::Background});
  check(entered.get_future().wait_for(2s) == std::future_status::ready, "background job started");
  auto second = pool.submit([opened = gate.opened](std::stop_token) { opened.wait(); return 2; },
                            {.priority = Priority::Background});
  auto foreground = pool.submit([](std::stop_token) { return 3; });
  const bool ran = foreground.wait_for(2s);
  check(ran, "foreground runs while background holds every background worker");
  check(second.status() == Status::Pending, "second background job waits for a background worker");
  gate.open();
  check(first.wait_for(2s) && second.wait_for(2s), "background jobs finish after release");
  check(first.take() == 1 && second.take() == 2 && foreground.take() == 3, "all values delivered");
}

static void queue_is_bounded_and_foreground_overtakes_background() {
  Executor pool({.threads = 1, .queue_capacity = 4, .background_queue_capacity = 3});
  Gate gate;
  std::promise<void> entered;
  auto running = pool.submit([&entered, opened = gate.opened](std::stop_token) {
    entered.set_value();
    opened.wait();
    return 0;
  });
  check(entered.get_future().wait_for(2s) == std::future_status::ready, "blocking job started");
  std::vector<int> order;
  std::vector<Handle<int>> pending;
  for (int i = 0; i < 3; ++i)
    pending.push_back(pool.submit([&order, i](std::stop_token) { order.push_back(i); return i; },
                                  {.priority = Priority::Background}));
  bool ran_rejected = false;
  auto rejected_background = pool.submit([&](std::stop_token) { ran_rejected = true; return 9; },
                                         {.priority = Priority::Background});
  check(!rejected_background.accepted() && rejected_background.status() == Status::Rejected,
        "background is rejected at its reserved queue bound");
  auto foreground = pool.submit([&order](std::stop_token) { order.push_back(3); return 3; });
  check(foreground.accepted(), "foreground still fits in the reserved queue slot");
  auto rejected = pool.submit([&](std::stop_token) { ran_rejected = true; return 9; });
  check(rejected.status() == Status::Rejected, "foreground is rejected at the total bound");
  check(pool.queued() == 4, "queued count reports the bound");
  gate.open();
  bool all_done = foreground.wait_for(2s);
  for (auto &job : pending) all_done = job.wait_for(2s) && all_done;
  check(all_done, "accepted jobs finish");
  pool.shutdown();
  check(order == std::vector<int>({3, 0, 1, 2}), "foreground overtakes queued background");
  check(!ran_rejected, "rejected jobs never run");
}

static void cancellation() {
  Executor pool({.threads = 1});
  Gate gate;
  std::promise<void> entered;
  auto blocker = pool.submit([&entered, opened = gate.opened](std::stop_token) {
    entered.set_value();
    opened.wait();
    return 0;
  });
  check(entered.get_future().wait_for(2s) == std::future_status::ready, "blocker started");
  std::atomic<bool> queued_ran{false};
  auto queued = pool.submit([&](std::stop_token) { queued_ran = true; return 1; });
  queued.cancel();
  check(queued.status() == Status::Cancelled, "cancelling a queued job settles it immediately");
  gate.open();
  check(blocker.wait_for(2s), "blocker finished");

  std::promise<void> running_entered;
  auto running = pool.submit([&running_entered](std::stop_token stop) {
    running_entered.set_value();
    while (!stop.stop_requested()) std::this_thread::yield();
    return 2;
  });
  check(running_entered.get_future().wait_for(2s) == std::future_status::ready, "running job started");
  running.cancel();
  check(running.wait_for(2s) && running.status() == Status::Cancelled,
        "a running job that observes its token finishes Cancelled");
  check(!running.take().has_value(), "a cancelled job delivers no value");

  // Dropping a handle cancels; a must-complete job ignores cancel().
  std::promise<void> dropped_entered;
  std::atomic<bool> dropped_saw_stop{false};
  {
    auto dropped = pool.submit([&](std::stop_token stop) {
      dropped_entered.set_value();
      while (!stop.stop_requested()) std::this_thread::yield();
      dropped_saw_stop = true;
      return 3;
    });
    check(dropped_entered.get_future().wait_for(2s) == std::future_status::ready, "dropped job started");
  }
  auto write = pool.submit([](std::stop_token stop) { return stop.stop_requested() ? -1 : 4; },
                           {.must_complete = true});
  write.cancel();
  check(write.wait_for(2s) && write.take() == 4, "must-complete job ignores cancel()");
  check(dropped_saw_stop, "destroying a handle requests stop");
  pool.shutdown();
  check(!queued_ran, "a cancelled queued job never runs");
}

static void shutdown_cancels_reads_and_drains_must_complete() {
  Executor pool({.threads = 1, .queue_capacity = 8});
  std::promise<void> entered;
  auto read = pool.submit([&entered](std::stop_token stop) {
    entered.set_value();
    while (!stop.stop_requested()) std::this_thread::yield();
    return 1;
  });
  check(entered.get_future().wait_for(2s) == std::future_status::ready, "read started");
  std::atomic<bool> queued_read_ran{false};
  auto queued_read = pool.submit([&](std::stop_token) { queued_read_ran = true; return 2; },
                                 {.priority = Priority::Background});
  auto write = pool.submit([](std::stop_token stop) { return stop.stop_requested() ? -1 : 3; },
                           {.priority = Priority::Background, .must_complete = true});
  pool.shutdown();
  check(read.status() == Status::Cancelled, "shutdown cancels the running read");
  check(queued_read.status() == Status::Cancelled && !queued_read_ran,
        "shutdown cancels queued reads without running them");
  check(write.status() == Status::Completed && write.take() == 3,
        "shutdown runs accepted must-complete work with an unstopped token");
  pool.shutdown();
  auto late = pool.submit([](std::stop_token) { return 4; }, {.must_complete = true});
  check(late.status() == Status::Rejected, "submission after shutdown is rejected");
}

static void tickets_reject_stale_completions() {
  Executor pool({.threads = 1});
  Generation view(42);
  auto first = pool.submit([](std::stop_token) { return 1; }, {.ticket = view.issue()});
  auto second = pool.submit([](std::stop_token) { return 2; }, {.ticket = view.issue()});
  check(first.wait_for(2s) && second.wait_for(2s), "both requests completed");
  check(first.ticket().owner == 42, "ticket carries the owner");
  check(!view.is_current(first.ticket()), "a superseded request is stale");
  check(view.is_current(second.ticket()), "the latest request is current");
  view.invalidate();
  check(!view.is_current(second.ticket()), "invalidate() makes every outstanding ticket stale");
  Generation other(7);
  check(!other.is_current(Ticket{42, 0}), "another owner's ticket is never current");
}

static void busy_delay() {
  using Clock = BusyDelay::Clock;
  const auto t0 = Clock::time_point{} + 1s;
  BusyDelay busy;
  check(!busy.visible(t0), "idle is not visible");
  busy.start(t0);
  check(!busy.visible(t0 + 149ms), "not visible before the delay");
  check(busy.visible(t0 + 150ms), "visible once the delay elapses");
  busy.start(t0 + 140ms);
  check(busy.visible(t0 + 150ms), "a second start keeps the first start time");
  busy.stop();
  check(!busy.visible(t0 + 1s), "stop hides it");
  busy.delay = 0ms;
  busy.start(t0);
  check(busy.visible(t0), "a zero delay shows immediately");
}

int main() {
  printf("=== jobs tests ===\n\n");
  completes_and_fails();
  foreground_has_a_reserved_worker();
  queue_is_bounded_and_foreground_overtakes_background();
  cancellation();
  shutdown_cancels_reads_and_drains_must_complete();
  tickets_reject_stale_completions();
  busy_delay();
  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
