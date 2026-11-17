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
#include <filesystem>
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
  // The packer is the part that fails quietly: raylib's row packer drops
  // glyphs on a large atlas and only says so in a log line. The library's own
  // default_codepoints() is 556, so the shipped default is the likeliest thing
  // to hit it, which is why this defaults to skyline.
  //
  // Pins the defaults, not raylib's packing: proving glyphs survive needs a
  // real font file and the library has none to test against.
  {
    const afterhours::FontAtlasConfig atlas_defaults;
    printf("  atlas defaults: padding %d, packer %d\n", atlas_defaults.padding,
           (int)atlas_defaults.packer);
    check(atlas_defaults.packer == afterhours::FontAtlasPacker::Skyline,
          "atlas packer defaults to skyline");
    // An app changes it for every load in one call, and fields it does not
    // name keep their defaults.
    afterhours::set_font_atlas_config({.padding = 9});
    check(afterhours::default_font_atlas_config().padding == 9,
          "set_font_atlas_config takes");
    check(afterhours::default_font_atlas_config().packer ==
              afterhours::FontAtlasPacker::Skyline,
          "and an unnamed field keeps its default");
    afterhours::set_font_atlas_config({});
    // 1px bleeds between glyphs when an app downscales the atlas.
    check(atlas_defaults.padding >= 4, "atlas padding survives downscaling");
  }

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

  {
    raylib::GlyphInfo glyph{};
    glyph.value = '0';
    glyph.advanceX = 12;
    raylib::Rectangle rec{};
    rec.width = 9;
    raylib::Font fake{};
    fake.texture.id = 1;
    fake.baseSize = 32;
    fake.glyphCount = 1;
    fake.glyphs = &glyph;
    fake.recs = &rec;
    check(afterhours::monospace_cell_advance(fake, 32.f) == 12.f,
          "cell advance is the digit glyph advance at base size");
    check(afterhours::monospace_cell_advance(fake, 16.f) == 6.f,
          "cell advance scales with the requested size");
    glyph.advanceX = 0;
    check(afterhours::monospace_cell_advance(fake, 32.f) == 9.f,
          "cell advance falls back to the glyph rect when advance is zero");
    glyph.advanceX = 12;
    check(afterhours::monospace_cell_advance(fake, 0.f) == 0.f,
          "cell advance rejects a zero size");
    const raylib::Font none = raylib::GetFontDefault();
    check(afterhours::monospace_cell_advance(none, 20.f) == 10.f,
          "cell advance estimates like measure_text without an atlas");
  }

  // --- loaders must not hand back an atlas-less font ---------------------
  {
    const auto path = std::filesystem::path(AFTERHOURS_TEST_FIXTURES) /
                      "AtkinsonHyperlegible-Regular.ttf";
    const raylib::Font loaded =
        afterhours::load_font_for_string("hello", path.string());
    check(loaded.glyphCount > 0 && loaded.recs != nullptr,
          "load_font_for_string returns glyph data without a window");
    check(!afterhours::font_is_usable(loaded),
          "and no texture is claimed: there is no GL context to make one");

    const raylib::Font from_file =
        afterhours::load_font_from_file(path.string().c_str());
    check(from_file.glyphCount > 0 && !afterhours::font_is_usable(from_file),
          "load_font_from_file returns glyphs without a window, no crash");
    int cps[] = {'a', 'b'};
    const raylib::Font with_cps = afterhours::load_font_from_file_with_codepoints(
        path.string().c_str(), cps, 2);
    check(with_cps.glyphCount == 2 && !afterhours::font_is_usable(with_cps),
          "load_font_from_file_with_codepoints too");
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
