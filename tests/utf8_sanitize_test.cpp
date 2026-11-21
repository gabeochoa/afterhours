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

static const std::string FFFD = "\xEF\xBF\xBD";

static size_t count_fffd(const std::string &s) {
  size_t n = 0;
  for (size_t i = 0; i + 3 <= s.size(); ++i)
    if (s.compare(i, 3, FFFD) == 0) n++;
  return n;
}

int main() {
  printf("Running utf8 sanitize tests...\n\n");

  check(sanitize_utf8("hello") == "hello", "ascii identical");
  check(sanitize_utf8("caf\xC3\xA9") == "caf\xC3\xA9", "2-byte identical");
  check(sanitize_utf8("\xF0\x9F\x91\x8D") == "\xF0\x9F\x91\x8D", "4-byte identical");
  check(sanitize_utf8("\xC2\x80") == "\xC2\x80", "smallest 2-byte identical");
  check(sanitize_utf8("\xF4\x8F\xBF\xBF") == "\xF4\x8F\xBF\xBF", "U+10FFFF identical");
  check(sanitize_utf8("") == "", "empty identical");
  check(sanitize_utf8("\xE2\x9C\x93 ok") == "\xE2\x9C\x93 ok", "3-byte identical");

  check(sanitize_utf8("a\x85" "b") == "a" + FFFD + "b", "lone continuation replaced in place");
  check(sanitize_utf8("\x80\x80") == FFFD + FFFD, "two lone continuations, two FFFD");

  check(sanitize_utf8("hi\xF0\x9F") == "hi" + FFFD, "truncated emoji tail is one FFFD");
  check(sanitize_utf8("x\xE2\x82") == "x" + FFFD, "truncated 3-byte tail is one FFFD");
  check(sanitize_utf8("x\xE2") == "x" + FFFD, "lone lead at end is one FFFD");

  check(sanitize_utf8("a\xE2\x82" "b") == "a" + FFFD + "b", "maximal subpart is one FFFD");
  check(sanitize_utf8("\xC3x") == FFFD + "x", "lead before ascii is one FFFD");

  check(sanitize_utf8("\xC0\x80") == FFFD + FFFD, "overlong NUL is two FFFD");
  check(sanitize_utf8("\xC1\xBF") == FFFD + FFFD, "overlong C1 lead is two FFFD");
  check(count_fffd(sanitize_utf8("\xE0\x80\x80")) == 3, "overlong 3-byte is three FFFD");

  check(count_fffd(sanitize_utf8("\xED\xA0\x80")) == 3, "surrogate is three FFFD");
  check(count_fffd(sanitize_utf8("\xF4\x90\x80\x80")) == 4, "above U+10FFFF is four FFFD");
  check(sanitize_utf8("\xF5\x88") == FFFD + FFFD, "lead above F4 is invalid per byte");

  check(is_valid_utf8("hello caf\xC3\xA9 \xF0\x9F\x91\x8D"), "valid agrees");
  check(is_valid_utf8(""), "empty is valid");
  check(!is_valid_utf8("a\x85"), "lone continuation invalid");
  check(!is_valid_utf8("x\xE2\x82"), "truncated invalid");
  check(!is_valid_utf8("\xC0\x80"), "overlong invalid");
  check(!is_valid_utf8("\xED\xA0\x80"), "surrogate invalid");

  const std::string messy = "a\x85\xE2\x82""b\xC0\x80\xF0\x9F";
  const std::string once = sanitize_utf8(messy);
  check(is_valid_utf8(once), "sanitized output is valid");
  check(sanitize_utf8(once) == once, "sanitize is idempotent");

  printf("%d/%d checks passed\n", checks_passed, checks_run);
  return checks_run == checks_passed ? 0 : 1;
}
