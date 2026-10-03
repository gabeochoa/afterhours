#include <afterhours/src/frame_loop.h>

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

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

int main() {
  printf("Running frame_loop tests...\n\n");

  using namespace afterhours::frame_loop;

  check(mode() == Mode::Continuous, "continuous is the default mode");
  check(!frame_requested(), "no frame is requested at startup");
  check(wait_for_frame_request(0), "continuous mode never blocks the host");

  set_mode(Mode::OnDemand);
  check(mode() == Mode::OnDemand, "mode switches to on-demand");
  check(!frame_requested(), "switching mode grants no frame by itself");
  check(!consume_frame_request(), "nothing to consume before a request");

  request_frame();
  request_frame();
  request_frame();
  check(frame_requested(), "a request is pending");
  check(stats().requests == 3, "every request is counted");
  check(consume_frame_request(), "the pending request grants one frame");
  check(!consume_frame_request(), "coalesced requests grant one frame only");

  const auto slow_start = std::chrono::steady_clock::now();
  check(!wait_for_frame_request(30), "an idle wait times out ungranted");
  const auto slow_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - slow_start)
                           .count();
  check(slow_ms >= 25, "the wait actually blocked instead of spinning");

  std::thread worker([] {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    request_frame();
  });
  const auto wake_start = std::chrono::steady_clock::now();
  check(wait_for_frame_request(5000), "a worker request wakes the host");
  const auto wake_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - wake_start)
                           .count();
  worker.join();
  check(wake_ms < 1000, "the wake lands promptly, not at the timeout");
  check(consume_frame_request(), "the woken host can consume the frame");

  note_frame(true);
  note_frame(false);
  check(stats().frames_run == 1, "ran frames are counted");
  check(stats().frames_skipped == 1, "skipped frames are counted");

  set_mode(Mode::Continuous);
  check(wait_for_frame_request(0), "continuous mode grants without a request");

  reset_stats();
  check(stats().requests == 0 && stats().frames_run == 0 &&
            stats().frames_skipped == 0,
        "reset clears the counters");
  check(!frame_requested(), "reset leaves no pending request behind");
  set_mode(Mode::Continuous);

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
