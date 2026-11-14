// default_font_test.cpp
// set_default_font replaces raylib's built-in pixel face wherever the library
// falls back to "no font": get_default_font, get_unset_font, and the UI
// plugin's default and unset fonts. No window: the font is a plain struct.

#include <cstdio>
#include <string>

#include <afterhours/ah.h>
#include <afterhours/src/drawing_helpers.h>

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
  raylib::Font builtin = raylib::GetFontDefault();
  check(get_default_font().baseSize == builtin.baseSize &&
            get_default_font().glyphCount == builtin.glyphCount,
        "unset, the default is raylib's built-in font");

  raylib::Font mono{};
  mono.baseSize = 42;
  mono.glyphCount = 7;
  set_default_font(mono);
  check(get_default_font().baseSize == 42, "set_default_font replaces the default");
  check(get_unset_font().baseSize == 42, "the unset font follows it");

  clear_default_font();
  check(get_default_font().baseSize == builtin.baseSize,
        "clear_default_font restores raylib's font");

  printf("default_font_test: %d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
