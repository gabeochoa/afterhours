#define AFTER_HOURS_USE_NONE
#include <afterhours/src/font_helper.h>

#include <cstdio>
#include <string>

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

int main() {
  const Font font{};
  const Vector2Type hello = measure_text(font, "hello", 20.f, 1.f);
  check(hello.x == 50.f && hello.y == 20.f,
        "with no measure fn, text is estimated at half its height per byte");
  check(measure_text(font, "hello world", 20.f, 1.f).x > hello.x,
        "a longer string estimates wider");
  check(measure_text(font, "", 20.f, 1.f).x == 0.f, "empty text is zero wide");
  check(measure_text(font, nullptr, 20.f, 1.f).x == 0.f, "null text is zero wide");

  set_measure_text_fn([](const char *, float size, float) {
    return Vector2Type{7.f, size};
  });
  check(measure_text(font, "hello", 20.f, 1.f).x == 7.f,
        "an installed measure fn still wins");
  set_measure_text_fn(nullptr);

  printf("%d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
