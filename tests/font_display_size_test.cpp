#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>

#include <afterhours/ah.h>
#include <afterhours/src/backends/raylib/font_helper.h>
#include <afterhours/src/graphics.h>

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
  int codepoints[] = {'A', 'B', 'C'};
  const int count = 3;

  graphics::render_scale() = 1;
  check(font_display_scale() == 1.f, "scale 1 at dpi 1");
  check(load_font_from_file(path.c_str()).baseSize == 32,
        "default load at dpi 1 is 32px");
  check(load_font_from_file(path.c_str(), 16).baseSize == 16,
        "explicit 16 honored at dpi 1");
  const raylib::Font at_1x = load_font_from_file(path.c_str());
  const auto measured_1x = measure_text(at_1x, "ABC", 20.f, 1.f);
  check(measured_1x.x > 0.f, "dpi 1 font measures");

  graphics::render_scale() = 2;
  check(font_display_scale() == 2.f, "scale 2 at dpi 2");
  check(display_scaled_size(32) == 64, "display_scaled_size(32) == 64");
  check(load_font_from_file(path.c_str()).baseSize == 64,
        "default load at dpi 2 is 64px");
  check(load_font_from_file(path.c_str(), 16).baseSize == 16,
        "explicit 16 unchanged at dpi 2");
  check(load_font_at_display_size(path.c_str(), 32).baseSize == 64,
        "load_font_at_display_size scales the nominal size");
  check(load_font_from_file_with_codepoints(path.c_str(), codepoints, count)
                .baseSize == 64,
        "with_codepoints default at dpi 2 is 64px");
  check(load_font_for_string("ABC", path.string()).baseSize == 192,
        "load_font_for_string default at dpi 2 is 192px");

  const raylib::Font at_2x = load_font_from_file(path.c_str());
  const auto measured_2x = measure_text(at_2x, "ABC", 20.f, 1.f);
  check(std::fabs(measured_2x.x - measured_1x.x) < 1.f &&
            std::fabs(measured_2x.y - measured_1x.y) < 1.f,
        "logical measure unchanged across dpi 1/2");

  graphics::render_scale() = 1;
  printf("font_display_size_test: %d/%d checks passed\n", checks_passed,
         checks_run);
  graphics::shutdown();
  return checks_passed == checks_run ? 0 : 1;
}
