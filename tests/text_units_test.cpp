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

static size_t graphemes(const std::string &s) {
  std::vector<UnitSpan> spans;
  split_graphemes(s, spans);
  return spans.size();
}

int main() {
  printf("Running text units tests...\n\n");

  check(graphemes("hello") == 5, "ascii: one grapheme per byte");
  check(graphemes("") == 0, "empty string has no units");
  check(graphemes("caf\xC3\xA9") == 4, "precomposed e-acute is one grapheme");
  check(graphemes("cafe\xCC\x81") == 4, "e + combining acute is one grapheme");
  check(graphemes("\xF0\x9F\x91\x8D\xF0\x9F\x8F\xBD") == 1, "thumbs up + skin tone is one grapheme");
  check(graphemes("\xF0\x9F\x87\xBA\xF0\x9F\x87\xB8") == 1, "regional indicator pair is one flag");
  check(graphemes("\xF0\x9F\x87\xBA\xF0\x9F\x87\xB8\xF0\x9F\x87\xAB\xF0\x9F\x87\xB7") == 2, "two flags stay two");
  check(graphemes("\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA7") == 1,
        "ZWJ family is one grapheme");
  check(graphemes("\xE2\x9D\xA4\xEF\xB8\x8F") == 1, "heart + variation selector is one grapheme");
  check(graphemes("a\xF0\x9F\x91\x8D" "b") == 3, "emoji between letters splits cleanly");

  {
    std::vector<UnitSpan> spans;
    split_graphemes("x\xCC\x81y", spans);
    check(spans.size() == 2 && spans[0].begin == 0 && spans[0].end == 3 && spans[1].begin == 3,
          "spans are byte offsets that cover the combining mark");
  }

  {
    std::vector<UnitSpan> words;
    split_words("  two   words\tand\nlines ", words);
    check(words.size() == 4, "words skip runs of whitespace");
    check(words[0].begin == 2 && words[0].end == 5, "word span excludes leading spaces");
    check(std::string_view("  two   words\tand\nlines ").substr(words[3].begin, words[3].size()) == "lines",
          "last word is exact");
  }

  {
    TextUnitCache cache;
    const auto &a = cache.get("one two", TextUnit::Word);
    const auto *ptr = &a;
    const auto &b = cache.get("one two", TextUnit::Word);
    check(&b == ptr && b.size() == 2, "same text and unit reuse the cached spans");
    const auto &c = cache.get("one two", TextUnit::Char);
    check(c.size() == 7, "changing the unit re-splits");
    const auto &d = cache.get("three", TextUnit::Char);
    check(d.size() == 5, "changing the text re-splits");
  }

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
