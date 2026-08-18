// headless_fallback_test.cpp
// What the engine reports when there is no window at all.
//
// Deliberately never calls InitWindow. That is the state kart-afterhours runs
// in, and it is the one wm_afterhours cannot reproduce -- its --headless mode
// still builds a real GL context, so GetRenderWidth() and the default font
// both work there and these paths never run.
//
// Without the fallbacks: the render size is 0, so every layout collapses to
// nothing, and MeasureTextEx returns {0,0} for the atlas-less default font, so
// every text-sized widget does too. Both silently.

#include <cstdio>
#include <string>

#include <afterhours/src/backends/raylib/font_helper.h>
#include <afterhours/src/plugins/window_manager.h>
#include <afterhours/src/shutdown.h>

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
  printf("Running headless fallback tests...\n\n");

  using WM = afterhours::window_manager;

  // --- resolution ---------------------------------------------------------
  {
    const auto rez = WM::fetch_current_resolution();
    check(rez.width > 0 && rez.height > 0,
          "no window still reports a usable resolution, not (0,0)");
    check(rez.width == 1280 && rez.height == 720,
          "and it is the documented 1280x720 default");

    WM::headless_resolution = WM::Resolution{1920, 1080};
    const auto overridden = WM::fetch_current_resolution();
    check(overridden.width == 1920 && overridden.height == 1080,
          "headless_resolution is what the fallback honours");
    WM::headless_resolution = WM::Resolution{1280, 720};
  }

  // --- fonts --------------------------------------------------------------
  {
    // GetFontDefault()'s atlas is built by InitWindow, so this one has none.
    const raylib::Font none = raylib::GetFontDefault();
    check(!afterhours::font_is_usable(none),
          "the default font is correctly seen as unusable without a window");

    raylib::Font fake{};
    fake.texture.id = 1;
    fake.glyphCount = 1;
    raylib::Rectangle rec{};
    fake.recs = &rec;
    check(afterhours::font_is_usable(fake),
          "a font with an atlas is usable");
  }

  // --- measurement degrades instead of returning zero ---------------------
  {
    const raylib::Font none = raylib::GetFontDefault();
    const auto sz = afterhours::measure_text(none, "hello", 20.f, 1.f);
    check(sz.x > 0.f && sz.y > 0.f,
          "text still measures to something, so layout does not collapse");
    check(sz.y == 20.f, "estimated height is the requested font size");

    const auto wider = afterhours::measure_text(none, "hello world", 20.f, 1.f);
    check(wider.x > sz.x, "a longer string estimates wider");

    const auto empty = afterhours::measure_text(none, "", 20.f, 1.f);
    check(empty.x == 0.f, "an empty string is still zero wide");
  }

  // --- ordered teardown ---------------------------------------------------
  {
    afterhours::shutdown();
    afterhours::shutdown();
    check(true, "shutdown is idempotent and safe with no backend running");
  }

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
