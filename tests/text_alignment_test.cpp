// Text-layout (alignment) helpers from the pong gap file, under the none
// backend: the helpers record their draws, so the test asserts on the
// positions and colours of the recorded calls.

#define FMT_HEADER_ONLY
#define AFTER_HOURS_USE_NONE
#include <fmt/format.h>

#include <afterhours/ah.h>
#include <afterhours/src/drawing_helpers.h>

#include <cmath>
#include <cstdio>
#include <cstring>

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

static void check_near(float actual, float expected, const char *what) {
  ++checks;
  if (std::abs(actual - expected) > 0.01f) {
    ++failures;
    std::printf("  FAIL: %s: expected %.3f, got %.3f\n", what, expected,
                actual);
  }
}

int main() {
  // 10px per char at any size, via the none measure stub.
  set_measure_text_fn([](const char *text, float size, float) {
    return Vector2Type{static_cast<float>(std::strlen(text)) * 10.f, size};
  });
  const Font font{};
  const Color white{255, 255, 255, 255};
  const Color black{0, 0, 0, 255};

  clear_draw_calls();
  draw_text_centered(font, "HI", 100.f, 50.f, 20.f, white);
  check(capture::calls().size() == 1, "centered draws once");
  if (capture::calls().size() == 1) {
    const auto &call = capture::calls()[0];
    check(call.op == "text", "centered records text");
    check_near(call.rect.x, 90.f, "centered x is centre minus half width");
    check_near(call.rect.y, 50.f, "centered y is unchanged");
    check(call.text == "HI", "centered records the text");
    check(call.color.r == 255 && call.color.a == 255,
          "centered keeps the colour");
  }

  clear_draw_calls();
  draw_text_right_aligned(font, "HI", 100.f, 50.f, 20.f, white);
  check(capture::calls().size() == 1, "right-aligned draws once");
  if (capture::calls().size() == 1)
    check_near(capture::calls()[0].rect.x, 80.f,
               "right-aligned x is right edge minus width");

  clear_draw_calls();
  draw_text_with_shadow(font, "HI", 10.f, 20.f, 20.f, white, black);
  check(capture::calls().size() == 2, "shadow draws twice");
  if (capture::calls().size() == 2) {
    check_near(capture::calls()[0].rect.x, 12.f,
               "shadow pass is offset right by default");
    check_near(capture::calls()[0].rect.y, 22.f,
               "shadow pass is offset down by default");
    check(capture::calls()[0].color.r == 0,
          "shadow pass uses the shadow colour");
    check_near(capture::calls()[1].rect.x, 10.f,
               "main pass is at the requested position");
    check(capture::calls()[1].color.r == 255,
          "main pass uses the main colour");
  }

  clear_draw_calls();
  draw_text_centered(font, nullptr, 100.f, 50.f, 20.f, white);
  draw_text_with_shadow(font, nullptr, 10.f, 20.f, 20.f, white, black);
  check(capture::calls().empty(), "null text draws nothing");

  std::printf("%d/%d checks passed\n", checks - failures, checks);
  if (failures > 0) {
    std::printf("%d checks FAILED\n", failures);
    return 1;
  }
  std::printf("All checks passed!\n");
  return 0;
}
