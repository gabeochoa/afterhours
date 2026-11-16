#include <afterhours/src/plugins/ui/text_units.h>

#include <cstdio>
#include <string>

using namespace afterhours::ui;

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

static float ten_per_byte(const std::string &s) {
  return 10.f * static_cast<float>(s.size());
}

int main() {
  printf("Running text fit tests...\n\n");

  check(ellipsize_text("hello", 50.f, ten_per_byte) == "hello",
        "text that fits comes back unchanged");
  check(ellipsize_text("hello world", 60.f, ten_per_byte) == "hel...",
        "longest prefix plus ellipsis fits the width");
  check(ten_per_byte(ellipsize_text("hello world", 60.f, ten_per_byte)) <= 60.f,
        "ellipsized result never exceeds the width");
  check(ellipsize_text("hello world", 20.f, ten_per_byte) == "...",
        "no room for any prefix gives the bare ellipsis");
  check(ellipsize_text("hello world", 40.f, ten_per_byte, "\xE2\x80\xA6") ==
            "h\xE2\x80\xA6",
        "custom ellipsis is measured and appended");

  const std::string thumbs = "ab\xF0\x9F\x91\x8D" "cd";
  check(ellipsize_text(thumbs, 70.f, ten_per_byte) == "ab...",
        "cut never lands inside a multibyte emoji");
  const std::string family =
      "a\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9" "bcdef";
  check(fit_text_prefix(family, 100.f, ten_per_byte) == 1,
        "fit stops before a ZWJ sequence that does not fit whole");
  check(fit_text_prefix(family, 120.f, ten_per_byte) == 12,
        "fit keeps a ZWJ sequence that fits whole");
  check(fit_text_prefix("", 100.f, ten_per_byte) == 0, "empty text fits zero");
  check(fit_text_prefix("abc", 0.f, ten_per_byte) == 0,
        "zero width fits nothing");

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
