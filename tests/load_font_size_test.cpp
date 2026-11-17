#include <cstdio>
#include <filesystem>
#include <string>

#include <afterhours/ah.h>
#include <afterhours/src/graphics.h>
#include <afterhours/src/plugins/ui/ui_core_components.h>

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
  graphics::Config config;
  config.display = graphics::DisplayMode::Headless;
  config.width = 64;
  config.height = 64;
  if (!graphics::init(config))
    return 2;
  const auto path = std::filesystem::path(AFTERHOURS_TEST_FIXTURES) /
                    "AtkinsonHyperlegible-Regular.ttf";
  int codepoints[] = {'A', 'B', 0xE9};
  const int count = 3;

  ui::FontManager fm;
  fm.load_font_with_codepoints("big", path.c_str(), codepoints, count, 64);
  check(fm.get_font("big").baseSize == 64,
        "explicit size 64 reaches the loaded font");
  check(fm.get_font("big").glyphCount > 0, "sized load returns glyphs");

  fm.load_font_with_codepoints("small", path.c_str(), codepoints, count, 16);
  check(fm.get_font("small").baseSize == 16,
        "explicit size 16 reaches the loaded font");

  fm.load_font_with_codepoints("plain", path.c_str(), codepoints, count);
  check(fm.get_font("plain").baseSize == 32,
        "omitted size keeps the previous 32px load");

  printf("load_font_size_test: %d/%d checks passed\n", checks_passed,
         checks_run);
  graphics::shutdown();
  return checks_passed == checks_run ? 0 : 1;
}
