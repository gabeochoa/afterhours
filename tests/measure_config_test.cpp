// measure_config must answer what the layout would have answered.
//
// The point of it is that a windowing consumer can prefix-sum item heights
// without building the items, so the number it gives has to be the number the
// box model produces -- otherwise every spacer in every windowed list is
// quietly the wrong size and nothing fails to compile.
#define FMT_HEADER_ONLY
#include <fmt/format.h>

#include <afterhours/ah.h>
#include <afterhours/src/plugins/ui/measure_config.h>

#include <cmath>
#include <cstdio>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

static int tests_run = 0;
static int tests_passed = 0;

static void check(bool cond, const char *expr, const char *file, int line) {
  tests_run++;
  if (cond) {
    tests_passed++;
  } else {
    fprintf(stderr, "  FAIL: %s  (%s:%d)\n", expr, file, line);
  }
}

#define CHECK(expr) check((expr), #expr, __FILE__, __LINE__)
static bool near(float a, float b) { return std::fabs(a - b) < 0.01f; }

static void test_pixels_are_themselves() {
  auto m = measure_config(
      ComponentConfig{}.with_size(ComponentSize{pixels(120.f), pixels(40.f)}),
      1000.f, 1000.f);
  CHECK(near(m.size.x, 120.f));
  CHECK(near(m.size.y, 40.f));
}

static void test_percent_is_of_what_you_pass_in() {
  auto m = measure_config(ComponentConfig{}.with_size(ComponentSize{
                              percent(0.5f), percent(0.25f)}),
                          400.f, 800.f);
  CHECK(near(m.size.x, 200.f));
  CHECK(near(m.size.y, 200.f));
}

// The thing #224 actually asked for: pitch, so a column can be prefix-summed.
static void test_pitch_includes_margin() {
  auto m = measure_config(
      ComponentConfig{}
          .with_size(ComponentSize{pixels(100.f), pixels(30.f)})
          .with_margin(Margin{.top = pixels(4.f), .bottom = pixels(6.f)}),
      500.f, 500.f);
  CHECK(near(m.size.y, 30.f));
  CHECK(near(m.margin.y, 10.f));
  CHECK(near(m.pitch().y, 40.f));
}

// Children and Expand are claims about siblings that do not exist yet.
static void test_untreeable_dims_are_zero() {
  auto m = measure_config(
      ComponentConfig{}.with_size(ComponentSize{children(), expand()}), 500.f,
      500.f);
  CHECK(near(m.size.x, 0.f));
  CHECK(near(m.size.y, 0.f));
}

static void test_margin_percent_is_of_available() {
  auto m = measure_config(ComponentConfig{}
                              .with_size(ComponentSize{pixels(10.f),
                                                       pixels(10.f)})
                              .with_margin(Margin{.left = percent(0.1f),
                                                  .right = percent(0.1f)}),
                          200.f, 200.f);
  CHECK(near(m.margin.x, 40.f));
}

int main() {
  printf("Running measure_config tests...\n\n");
  printf("  pixels_are_themselves\n");
  test_pixels_are_themselves();
  printf("  percent_is_of_what_you_pass_in\n");
  test_percent_is_of_what_you_pass_in();
  printf("  pitch_includes_margin\n");
  test_pitch_includes_margin();
  printf("  untreeable_dims_are_zero\n");
  test_untreeable_dims_are_zero();
  printf("  margin_percent_is_of_available\n");
  test_margin_percent_is_of_available();

  printf("\n%d/%d tests passed.\n", tests_passed, tests_run);
  if (tests_passed != tests_run) {
    printf("SOME TESTS FAILED!\n");
    return 1;
  }
  printf("ALL TESTS PASSED.\n");
  return 0;
}
