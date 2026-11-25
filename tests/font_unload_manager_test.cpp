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
  if (cond)
    checks_passed++;
  else
    fprintf(stderr, "  FAIL: %s\n", what.c_str());
}

int main() {
  graphics::Config config;
  config.display = graphics::DisplayMode::Headless;
  config.width = 64;
  config.height = 64;
  if (!graphics::init(config))
    return 2;
  const auto path = (std::filesystem::path(AFTERHOURS_TEST_FIXTURES) /
                     "AtkinsonHyperlegible-Regular.ttf")
                        .string();

  ui::FontManager fm;
  for (int i = 0; i < 17; i++)
    fm.load_font("face" + std::to_string(i), path.c_str());
  check(fm.fonts.size() == 17, "17 fonts registered (old sokol cap was 16)");
  check(fm.get_font("face16").glyphCount > 0, "17th font has glyphs");

  check(!fm.unload_font("nope"), "unloading a missing font returns false");
  check(fm.unload_font("face3"), "unload registered font");
  check(fm.fonts.size() == 16, "unload frees the registry slot");
  check(fm.get_font("face3").glyphCount > 0,
        "lookup after unload falls back, never dangles");

  fm.load_font("face3", path.c_str());
  check(fm.fonts.size() == 17 && fm.get_font("face3").glyphCount > 0,
        "reload after unload works");

  fm.load_font("alias3", fm.get_font("face3"));
  check(fm.unload_font("face3"), "unload one alias");
  check(fm.get_font("alias3").glyphCount > 0, "aliased face stays loaded");
  check(fm.unload_font("alias3"), "unload last alias frees the face");

  printf("font_unload_manager_test: %d/%d checks passed\n", checks_passed,
         checks_run);
  graphics::shutdown();
  return checks_passed == checks_run ? 0 : 1;
}
