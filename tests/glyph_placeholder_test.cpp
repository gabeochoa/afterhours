#define FMT_HEADER_ONLY
#if !defined(AFTER_HOURS_USE_RAYLIB) && !defined(AFTER_HOURS_USE_VULKAN) && !defined(AFTER_HOURS_USE_SOKOL)
#define AFTER_HOURS_USE_SOKOL
#endif
#include <fmt/format.h>
#include <cstdio>
#include <filesystem>
#include <string>
#define FONTSTASH_IMPLEMENTATION
#include <fontstash/fontstash.h>
#undef FONTSTASH_IMPLEMENTATION
#include <sokol/sokol_gfx.h>
#include <afterhours/src/backends/sokol/font_helper.h>

extern "C" float sapp_dpi_scale(void) { return 1.f; }
static int failures = 0;
#define CHECK(condition) do { if (!(condition)) { std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); ++failures; } } while (0)

static std::string visible(const char *text, bool has_fffd) {
  std::string out;
  const auto covered = [&](uint32_t cp) {
    return cp < 0x2000 || (has_fffd && cp == 0xFFFD);
  };
  return afterhours::font_coverage::visible_text(text, covered, out) ? out
                                                                     : text;
}

static void replacement_rules() {
  CHECK(visible("plain ascii", false) == "plain ascii");
  CHECK(visible("caf\xC3\xA9", false) == "caf\xC3\xA9");
  CHECK(visible("a\xE2\x98\x83" "b", false) == "a?b");
  CHECK(visible("a\xE2\x98\x83" "b", true) == "a\xEF\xBF\xBD" "b");
  CHECK(visible("\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA7", false) == "?");
  CHECK(visible("\xF0\x9F\x87\xBA\xF0\x9F\x87\xB8\xF0\x9F\x87\xAB\xF0\x9F\x87\xB7", false) == "??");
  CHECK(visible("\xF0\x9F\x91\x8D\xF0\x9F\x8F\xBD", false) == "?");
  CHECK(visible("x\xEF\xB8\x8F", false) == "x");
}

static void sokol_measures_the_placeholder() {
  namespace md = afterhours::graphics::metal_detail;
  FONSparams params{};
  params.width = params.height = 1024;
  params.flags = FONS_ZERO_TOPLEFT;
  md::g_fons_ctx = fonsCreateInternal(&params);
  const auto path = std::filesystem::path(AFTERHOURS_TEST_FIXTURES) / "AtkinsonHyperlegible-Regular.ttf";
  const afterhours::Font font = afterhours::load_font_from_file(path.c_str());
  CHECK(font.id != FONS_INVALID);
  CHECK(!afterhours::font_has_glyph(font, 0x2603));

  const std::string repl = afterhours::font_has_glyph(font, 0xFFFD) ? "\xEF\xBF\xBD" : "?";
  std::string storage;
  CHECK(afterhours::drawable_text(font.id, "a\xE2\x98\x83" "b", storage) == "a" + repl + "b");
  const char *ascii = "abc";
  CHECK(afterhours::drawable_text(font.id, ascii, storage) == ascii);

  const float with_snowman = afterhours::measure_text(font, "a\xE2\x98\x83" "b", 32.f, 1.f).x;
  const float with_repl = afterhours::measure_text(font, ("a" + repl + "b").c_str(), 32.f, 1.f).x;
  const float bare = afterhours::measure_text(font, "ab", 32.f, 1.f).x;
  CHECK(with_snowman == with_repl);
  CHECK(with_snowman > bare);
  fonsDeleteInternal(md::g_fons_ctx);
  md::g_fons_ctx = nullptr;
}

int main() {
  replacement_rules();
  sokol_measures_the_placeholder();
  if (failures) {
    std::fprintf(stderr, "%d failure(s)\n", failures);
    return 1;
  }
  std::printf("glyph_placeholder_test: all checks passed\n");
  return 0;
}
