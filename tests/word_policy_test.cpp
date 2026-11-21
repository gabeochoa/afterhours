#include <afterhours/src/plugins/ui/text_selection.h>

#include <cstdio>
#include <string>

using namespace afterhours::ui;
namespace ts = afterhours::ui::text_selection;

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

static void check_range(std::pair<size_t, size_t> got, size_t start, size_t end,
                        const std::string &what) {
  check(got.first == start && got.second == end,
        what + " got {" + std::to_string(got.first) + "," +
            std::to_string(got.second) + "} want {" + std::to_string(start) +
            "," + std::to_string(end) + "}");
}

int main() {
  printf("Running word policy tests...\n\n");

  check_range(ts::select_word_at("hello world", 1), 0, 5,
              "prose: hello");
  check_range(ts::select_word_at("hello world", 7), 6, 11,
              "prose: world");
  check_range(ts::select_word_at("don't stop", 1), 0, 3,
              "prose: don");
  check(ts::select_word_at("hello world", 5) ==
            std::make_pair<size_t, size_t>(5, 6),
        "prose: click on space selects the separator byte");

  check_range(ts::select_word_at("alpha_beta gamma", 7,
                                 ts::WordPolicy::Identifier),
              0, 10, "identifier: alpha_beta whole");
  check_range(ts::select_word_at("alpha_beta", 7, ts::WordPolicy::Prose),
              6, 10, "prose: alpha_beta splits at underscore");
  check_range(ts::select_word_at("foo.bar", 1,
                                 ts::WordPolicy::Identifier),
              0, 3, "identifier: dot still separates");

  const std::string uni = "alpha_\xc3\xa9\xce\xbb and";
  check_range(ts::select_word_at(uni, 6, ts::WordPolicy::Identifier), 0, 10,
              "identifier: unicode identifier whole");

  check_range(ts::select_word_at("src/foo/bar.cpp plus", 5,
                                 ts::WordPolicy::Path),
              0, 15, "path: src/foo/bar.cpp whole");
  check_range(ts::select_word_at("src/foo/bar.cpp", 5), 4, 7,
              "prose: same click gives foo");
  check_range(ts::select_word_at("dir name/file.txt", 11,
                                 ts::WordPolicy::Path),
              4, 17, "path: spaces still separate");
  check_range(ts::select_word_at("a b", 1, ts::WordPolicy::Path),
              1, 2, "path: click on space selects the separator byte");

  check(ts::find_word_end("alpha_beta gamma", 0,
                          ts::WordPolicy::Identifier) == 10,
        "find_word_end identifier");
  check(ts::find_word_start("alpha_beta gamma", 10,
                            ts::WordPolicy::Identifier) == 0,
        "find_word_start identifier");
  check(ts::find_word_end("src/foo", 0, ts::WordPolicy::Path) == 7,
        "find_word_end path");
  check(ts::find_word_start("src/foo", 7, ts::WordPolicy::Path) == 0,
        "find_word_start path");

  check_range(ts::select_word_at("", 0), 0, 0, "empty text");
  check_range(ts::select_word_at("hi", 99), 0, 2, "pos past end clamps");
  check(ts::find_word_start("", 0) == 0, "empty find_word_start");
  check(ts::find_word_end("", 0) == 0, "empty find_word_end");
  check(ts::is_word_char('_', ts::WordPolicy::Identifier) &&
            !ts::is_word_char('_', ts::WordPolicy::Prose) &&
            ts::is_word_char('/', ts::WordPolicy::Path) &&
            !ts::is_word_char('/', ts::WordPolicy::Prose) &&
            ts::is_word_char('\xc3', ts::WordPolicy::Identifier),
        "is_word_char per policy");

  printf("%d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
