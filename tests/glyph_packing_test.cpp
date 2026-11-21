#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>

#include <afterhours/ah.h>
#include <afterhours/src/graphics.h>
#include <afterhours/src/plugins/ui/ui_core_components.h>

using namespace afterhours;

static int failures = 0;
#define CHECK(condition)                                                          \
  do {                                                                            \
    if (!(condition)) {                                                           \
      std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition);           \
      ++failures;                                                                 \
    }                                                                             \
  } while (0)

static std::string captured;
static void capture(const char *, const char *message) {
  captured += message;
  captured += '\n';
}

static bool contains(const std::vector<uint32_t> &values, uint32_t needle) {
  return std::find(values.begin(), values.end(), needle) != values.end();
}

int main() {
  graphics::Config config;
  config.display = graphics::DisplayMode::Headless;
  config.width = 64;
  config.height = 64;
  if (!graphics::init(config))
    return 2;
  void (*previous_sink)(const char *, const char *) = log_sink_fn;
  log_sink_fn = capture;

  const auto path = std::filesystem::path(AFTERHOURS_TEST_FIXTURES) /
                    "AtkinsonHyperlegible-Regular.ttf";
  int codepoints[] = {'A', 'B', 0xE9};
  const int count = 3;

  // A clean load reports nothing and measures for real.
  ui::FontManager fm;
  fm.load_font_with_codepoints("clean", path.c_str(), codepoints, count, 32);
  const Font clean = fm.get_font("clean");
  CHECK(clean.glyphCount > 0);
  CHECK(font_is_usable(clean));
  CHECK(fm.packing_failures("clean").empty());
  CHECK(!fm.has_packing_failure("clean"));
  CHECK(measure_text(clean, "AB", 32.f, 1.f).x > 0.f);

  // Glyphs with no atlas texture (an atlas too large to upload) are their
  // own report, also once. Checked before the mutation below on purpose.
  Font notex = clean;
  notex.texture.id = 0;
  notex.baseSize = 999;
  captured.clear();
  check_font_atlas(notex, "notex");
  CHECK(captured.find("no texture") != std::string::npos);
  captured.clear();
  check_font_atlas(notex, "notex");
  CHECK(captured.find("no texture") == std::string::npos);

  // Force the failure the skyline packer would leave: a rasterized glyph
  // with an empty atlas rect. The glyph still measures, so text degrades
  // to a wrong corner glyph rather than disappearing.
  Font bad = clean;
  int idx = -1;
  for (int i = 0; i < bad.glyphCount; i++)
    if (bad.glyphs[i].value == 'B')
      idx = i;
  CHECK(idx >= 0);
  CHECK(bad.recs[idx].width > 0.f);
  bad.recs[idx] = {0, 0, 0, 0};
  CHECK(std::find(glyph_packing_failures(bad).begin(),
                  glyph_packing_failures(bad).end(),
                  'B') != glyph_packing_failures(bad).end());

  captured.clear();
  report_glyph_packing_failures(bad, "test", glyph_packing_failures(bad));
  CHECK(captured.find("U+0042") != std::string::npos);
  CHECK(fm.has_packing_failure("clean"));
  CHECK(contains(fm.packing_failures("clean"), static_cast<uint32_t>('B')));
  CHECK(!contains(fm.packing_failures("clean"), static_cast<uint32_t>('A')));
  const auto degraded = measure_text(clean, "BA", 32.f, 1.f);
  CHECK(std::isfinite(degraded.x) && degraded.x > 0.f);
  CHECK(std::isfinite(degraded.y) && degraded.y > 0.f);

  // Reporting the same glyph again stays silent.
  captured.clear();
  report_glyph_packing_failures(bad, "test", {'B'});
  CHECK(captured.find("U+0042") == std::string::npos);

  log_sink_fn = previous_sink;
  graphics::shutdown();
  if (failures)
    std::fprintf(stderr, "%d failure(s)\n", failures);
  else
    std::printf("glyph_packing_test: all checks passed\n");
  return failures ? 1 : 0;
}
