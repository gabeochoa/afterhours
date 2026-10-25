// synth::beep under the none backend: the stub logs
// "@notimplemented synth::beep" and returns an empty sound. The raylib
// path is covered by pong's build + e2e run, which initialises audio.

#define FMT_HEADER_ONLY
#define AFTER_HOURS_USE_NONE
#include <fmt/format.h>

#include <afterhours/ah.h>
#include <afterhours/src/plugins/audio_helpers.h>

#include <cstdio>

using namespace afterhours;

static int checks = 0;
static int failures = 0;

static void check(bool cond, const char *what) {
  ++checks;
  if (!cond) {
    ++failures;
    std::printf("  FAIL: %s\n", what);
  }
}

int main() {
  // Nothing observable in the stub beyond not crashing — including the
  // non-positive inputs the raylib path guards before touching audio.
  synth::beep(440.f, 0.1f);
  synth::beep(0.f, 0.1f);
  synth::beep(440.f, 0.f);
  synth::beep(-440.f, -1.f);
  check(true, "beep stub accepts valid and invalid inputs");

  std::printf("%d/%d checks passed\n", checks - failures, checks);
  if (failures > 0) {
    std::printf("%d checks FAILED\n", failures);
    return 1;
  }
  std::printf("All checks passed!\n");
  return 0;
}
