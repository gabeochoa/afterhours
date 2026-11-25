#include <afterhours/src/plugins/jobs.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

using namespace afterhours;

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

template <class Pred> static bool eventually(Pred pred) {
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (!pred()) {
    if (std::chrono::steady_clock::now() > deadline) return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return true;
}

struct Gate {
  std::atomic<bool> started = false;
  std::atomic<bool> open = false;
  std::atomic<bool> saw_stop = false;

  auto work() {
    return [this](std::stop_token stop) {
      started = true;
      while (!open && !stop.stop_requested())
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      saw_stop = stop.stop_requested();
      return 0;
    };
  }
};

static void delivers_on_owner_thread() {
  const auto main_id = std::this_thread::get_id();
  std::thread::id worker_id;
  std::thread::id delivered_on;
  int value = 0;
  jobs::Pool pool(2, 8);
  const auto requests_before = frame_loop::stats().requests;
  auto id = pool.submit(
      [](std::stop_token) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        return std::make_pair(std::this_thread::get_id(), 42);
      },
      [&](std::pair<std::thread::id, int> result) {
        worker_id = result.first;
        value = result.second;
        delivered_on = std::this_thread::get_id();
      });
  check(id.has_value(), "submit accepts while there is room");
  check(value == 0, "nothing is delivered before drain");
  check(eventually([&] { return pool.drain() > 0; }), "drain delivers");
  check(value == 42, "the result reaches the callback");
  check(worker_id != main_id, "work runs off the owner thread");
  check(delivered_on == main_id, "delivery runs on the owner thread");
  check(frame_loop::stats().requests > requests_before,
        "a finished job requests a frame");
}

static void bounds_the_queue() {
  Gate gate;
  auto noop = [](std::stop_token) { return 0; };
  auto ignore = [](int) {};
  jobs::Pool pool(1, 4);
  pool.submit(gate.work(), ignore);
  check(eventually([&] { return gate.started.load(); }), "blocker starts");
  int background_accepted = 0;
  for (int i = 0; i < 4; i++)
    if (pool.submit(noop, ignore, jobs::Priority::Background))
      background_accepted++;
  check(background_accepted == 3,
        "background work leaves a quarter of the queue for foreground");
  check(pool.submit(noop, ignore).has_value(),
        "foreground takes the reserved slot");
  check(!pool.submit(noop, ignore).has_value(), "a full queue rejects");
  gate.open = true;
}

static void foreground_first_and_reserved_worker() {
  {
    Gate gate;
    std::vector<char> order;
    jobs::Pool pool(1, 8);
    pool.submit(gate.work(), [](int) {});
    check(eventually([&] { return gate.started.load(); }), "blocker starts");
    pool.submit([](std::stop_token) { return 'b'; },
                [&](char c) { order.push_back(c); },
                jobs::Priority::Background);
    pool.submit([](std::stop_token) { return 'f'; },
                [&](char c) { order.push_back(c); });
    gate.open = true;
    eventually([&] {
      pool.drain();
      return order.size() == 2;
    });
    check(order == std::vector<char>{'f', 'b'},
          "queued foreground runs before queued background");
  }
  {
    Gate first, second;
    bool foreground_done = false;
    jobs::Pool pool(2, 8);
    pool.submit(first.work(), [](int) {}, jobs::Priority::Background);
    pool.submit(second.work(), [](int) {}, jobs::Priority::Background);
    pool.submit([](std::stop_token) { return 1; },
                [&](int) { foreground_done = true; });
    check(eventually([&] {
            pool.drain();
            return foreground_done;
          }),
          "foreground runs while background holds the other workers");
    check(eventually([&] { return first.started.load(); }),
          "the first background job runs");
    check(!second.started, "background never takes the last worker");
    first.open = true;
    second.open = true;
  }
}

static void cancels_at_every_stage() {
  Gate gate;
  int delivered = 0;
  auto count = [&](int) { delivered++; };
  std::atomic<bool> queued_ran = false;
  std::atomic<bool> finished = false;
  jobs::Pool pool(1, 8);
  auto running = pool.submit(gate.work(), count);
  check(eventually([&] { return gate.started.load(); }), "blocker starts");
  auto queued = pool.submit(
      [&](std::stop_token) {
        queued_ran = true;
        return 0;
      },
      count);
  pool.cancel(*queued);
  pool.cancel(*running);
  check(eventually([&] { return gate.saw_stop.load(); }),
        "a running job sees its stop token");

  auto done = pool.submit(
      [&](std::stop_token) {
        finished = true;
        return 0;
      },
      count);
  check(eventually([&] { return finished.load(); }), "next job finishes");
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  pool.cancel(*done);
  check(pool.drain() == 0, "cancelled jobs deliver nothing");
  check(delivered == 0, "no callback ran for a cancelled job");
  check(!queued_ran, "a job cancelled while queued never runs");
}

static void shuts_down_cleanly() {
  Gate gate;
  int delivered = 0;
  std::atomic<bool> queued_ran = false;
  jobs::Pool pool(1, 8);
  pool.submit(gate.work(), [&](int) { delivered++; });
  check(eventually([&] { return gate.started.load(); }), "blocker starts");
  pool.submit(
      [&](std::stop_token) {
        queued_ran = true;
        return 0;
      },
      [&](int) { delivered++; });
  pool.shutdown();
  check(gate.saw_stop, "shutdown stops the running job before joining");
  check(!queued_ran, "shutdown drops queued work");
  check(pool.drain() == 0, "nothing is delivered after shutdown");
  check(delivered == 0, "no callback runs after shutdown");
  check(!pool.submit([](std::stop_token) { return 0; }, [](int) {}),
        "submit after shutdown is rejected");

  std::atomic<bool> finished = false;
  jobs::Pool finished_pool(1, 8);
  finished_pool.submit(
      [&](std::stop_token) {
        finished = true;
        return 0;
      },
      [&](int) { delivered++; });
  eventually([&] { return finished.load(); });
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  finished_pool.shutdown();
  check(finished_pool.drain() == 0 && delivered == 0,
        "an undrained result is dropped by shutdown");
}

static void move_only_results() {
  int value = 0;
  jobs::Pool pool(1, 8);
  pool.submit([](std::stop_token) { return std::make_unique<int>(7); },
              [&](std::unique_ptr<int> p) { value = *p; });
  eventually([&] { return pool.drain() > 0; });
  check(value == 7, "a move-only result is handed over");
}

int main() {
  printf("Running jobs tests...\n\n");
  delivers_on_owner_thread();
  bounds_the_queue();
  foreground_first_and_reserved_worker();
  cancels_at_every_stage();
  shuts_down_cleanly();
  move_only_results();
  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
