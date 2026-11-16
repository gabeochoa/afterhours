#define FMT_HEADER_ONLY
#include <fmt/format.h>

#include <afterhours/ah.h>
#include <afterhours/src/plugins/ui.h>
#include <afterhours/src/plugins/ui/text_input/text_area_state.h>
#include <afterhours/src/plugins/ui/text_input/utils.h>

#include <cstdio>
#include <string>

using namespace afterhours;
using namespace afterhours::text_input;

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

static const std::string coder = "\xF0\x9F\x91\xA9\xF0\x9F\x8F\xBD\xE2\x80\x8D\xF0\x9F\x92\xBB";
static const std::string flag = "\xF0\x9F\x87\xBA\xF0\x9F\x87\xB8";
static const std::string e_acute = "e\xCC\x81";
static const std::string thumbs = "\xF0\x9F\x91\x8D\xF0\x9F\x8F\xBD";

static HasTextInputState field(const std::string &text, size_t cursor) {
  HasTextInputState s;
  s.storage.insert(0, text);
  s.cursor_position = cursor;
  return s;
}

static HasTextAreaState area(const std::string &text, size_t cursor) {
  HasTextAreaState s;
  s.storage.insert(0, text);
  s.rebuild_line_index();
  s.cursor_position = cursor;
  return s;
}

static float ten_per_byte(const std::string &s) {
  return 10.f * static_cast<float>(s.size());
}

int main() {
  const std::string text = "a" + coder + "b";
  CHECK(ui::next_grapheme_end(text, 1) == 1 + coder.size());
  CHECK(ui::prev_grapheme_start(text, 1 + coder.size()) == 1);
  CHECK(ui::prev_grapheme_start(text, 5) == 1);
  CHECK(ui::prev_grapheme_start(text, 0) == 0);
  CHECK(ui::next_grapheme_end(text, text.size()) == text.size());

  auto backspace = field(coder + " Priya", coder.size());
  CHECK(delete_before_cursor(backspace));
  CHECK(backspace.text() == " Priya");
  CHECK(backspace.cursor_position == 0);

  auto arrows = field(flag + "x", 0);
  move_cursor_right(arrows);
  CHECK(arrows.cursor_position == flag.size());
  move_cursor_left(arrows);
  CHECK(arrows.cursor_position == 0);

  auto forward = field(e_acute + "x", 0);
  CHECK(delete_at_cursor(forward));
  CHECK(forward.text() == "x");

  auto area_back = area("ab\n" + thumbs, 3 + thumbs.size());
  CHECK(delete_before_cursor_multiline(area_back));
  CHECK(area_back.text() == "ab\n");
  auto area_forward = area(thumbs + "\nab", 0);
  CHECK(delete_at_cursor_multiline(area_forward));
  CHECK(area_forward.text() == "\nab");

  CHECK(ui::text_selection::detail_sel::offset_nearest_x(thumbs + "x", 40.f, ten_per_byte) == 0);
  CHECK(ui::text_selection::detail_sel::offset_nearest_x(thumbs + "x", 50.f, ten_per_byte) == thumbs.size());

  for (const std::string &name : {std::string("Wi\xC5\x9Bniewska-Kowalczyk"),
                                 std::string("\xE7\x8E\x8B\xE7\xA7\x80\xE8\x8B\xB1\xE3\x81\xAE\xE8\xA8\xAD\xE5\xAE\x9A"),
                                 coder + " Priya"}) {
    for (float w = 30.f; w < 200.f; w += 10.f) {
      const std::string shown = ui::ellipsize_text(name, w, ten_per_byte);
      const size_t kept = shown.size() >= 3 && shown.compare(shown.size() - 3, 3, "...") == 0
                              ? shown.size() - 3
                              : shown.size();
      CHECK(kept == 0 || kept == name.size() ||
            ui::next_grapheme_end(name, ui::prev_grapheme_start(name, kept)) == kept);
    }
  }

  printf("%d/%d checks passed\n", tests_passed, tests_run);
  return tests_passed == tests_run ? 0 : 1;
}
