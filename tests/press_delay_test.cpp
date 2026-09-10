// press_delay_test.cpp
// An injected press is not readable on the frame it was injected. Undocumented
// and load-bearing; cartographer lost time to it twice. Pinning the timing.

#include <afterhours/src/plugins/e2e_testing/input_injector.h>

#include <cstdio>
#include <string>

namespace inj = afterhours::testing::input_injector;

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

static constexpr int kKey = 65; // 'A'

int main() {
  printf("=== synthetic press delay ===\n\n");

  inj::reset_frame();

  // The surprising one.
  {
    inj::set_key_down(kKey);
    const bool same_frame = inj::consume_press(kKey);
    printf("  same frame as key_down: %s\n", same_frame ? "seen" : "not seen");
    check(!same_frame,
          "a press is not readable on the frame it was injected");
  }

  {
    inj::reset_frame();
    const bool next_frame = inj::consume_press(kKey);
    printf("  one frame later:        %s\n", next_frame ? "seen" : "not seen");
    check(next_frame, "and it is readable on the next one");
  }

  // Action mappings sharing a key all have to see it.
  {
    check(inj::consume_press(kKey),
          "two readers in one frame both see the same press");
  }

  {
    inj::reset_frame();
    const bool third = inj::consume_press(kKey);
    printf("  two frames later:       %s\n", third ? "seen" : "not seen");
    check(!third, "the press is spent after its frame");
  }

  inj::set_key_up(kKey);
  inj::reset_frame();

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
