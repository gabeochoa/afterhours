// capture::record is the one buffer every backend writes to, so an app built
// against raylib or sokol can assert on what it drew instead of profiling a
// PNG. This runs on the `none` backend, which records unconditionally, so what
// is checked here is the gate and the buffer rather than any backend's wiring.

#include <cassert>
#include <cstdio>

#include "../src/capture.h"

static int checks = 0;
static int failures = 0;

static void check(bool cond, const char *what) {
  checks++;
  if (!cond) {
    failures++;
    std::printf("  FAIL: %s\n", what);
  }
}

int main() {
  using namespace afterhours;

  const RectangleType r{1.f, 2.f, 3.f, 4.f};
  const ColorType c{10, 20, 30, 255};

  // Off by default: a shipping build must not accumulate a draw log forever.
  capture::clear();
  capture::disable();
  capture::record("rectangle", r, c);
  check(capture::calls().empty(), "records nothing while disabled");

  capture::enable();
  capture::record("rectangle", r, c);
  capture::record("text", r, c, "hello");
  check(capture::calls().size() == 2, "records while enabled");

  const auto &text_call = capture::calls()[1];
  check(text_call.op == std::string("text"), "op is kept");
  check(text_call.text == std::string("hello"), "text is kept");
  check(text_call.rect.x == 1.f && text_call.rect.y == 2.f, "rect is kept");
  check(text_call.color.r == 10 && text_call.color.a == 255, "colour is kept");

  capture::clear();
  check(capture::calls().empty(), "clear empties the buffer");

  // Disabling mid-run stops recording without dropping what was already seen.
  capture::record("rectangle", r, c);
  capture::disable();
  capture::record("rectangle", r, c);
  check(capture::calls().size() == 1, "disable stops recording, keeps history");

  capture::disable();
  capture::clear();

  std::printf("%d/%d checks passed\n", checks - failures, checks);
  if (failures == 0) std::printf("All checks passed!\n");
  return failures == 0 ? 0 : 1;
}
