#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include <afterhours/ah.h>
#include <afterhours/src/drawing_helpers.h>
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
static void capture_log(const char *, const char *message) {
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
  log_sink_fn = capture_log;

  const auto path = std::filesystem::path(AFTERHOURS_TEST_FIXTURES) /
                    "AtkinsonHyperlegible-Regular.ttf";
  int codepoints[] = {'A', 'B', 0xE9};
  const int count = 3;

  ui::FontManager fm;
  fm.load_font_with_codepoints("clean", path.c_str(), codepoints, count, 32);
  const Font clean = fm.get_font("clean");
  CHECK(clean.glyphCount > 0);
  CHECK(font_is_usable(clean));
  CHECK(fm.packing_failures("clean").empty());
  CHECK(!fm.has_packing_failure("clean"));
  CHECK(measure_text(clean, "AB", 32.f, 1.f).x > 0.f);

  std::vector<unsigned char> bytes;
  CHECK(font_coverage::read_file(path.string().c_str(), bytes));
  const Font embedded = load_font_from_memory(
      bytes.data(), static_cast<int>(bytes.size()), "embedded");
  CHECK(embedded.glyphCount > 0);
  CHECK(embedded.texture.id != 0);
  CHECK(font_is_usable(embedded));
  CHECK(measure_text(embedded, "AB", 32.f, 1.f).x > 0.f);

  Font textureless = embedded;
  textureless.texture.id = 0;
  capture::enable();
  capture::clear();
  draw_text_ex(textureless, "hello", Vector2Type{0.f, 0.f}, 16.f, 0.f,
               Color{255, 255, 255, 255});
  CHECK(capture::calls().size() == 1 &&
        capture::calls()[0].op == std::string("text"));
  capture::clear();
  draw_text_ex(textureless, "hello", Vector2Type{0.f, 0.f}, 16.f, 0.f,
               Color{255, 255, 255, 255}, 30.f, 0.f, 0.f);
  CHECK(capture::calls().size() == 1);
  capture::disable();

  Font notex = clean;
  notex.texture.id = 0;
  notex.baseSize = 999;
  captured.clear();
  check_font_atlas(notex, "notex");
  CHECK(captured.find("no texture") != std::string::npos);
  captured.clear();
  check_font_atlas(notex, "notex");
  CHECK(captured.find("no texture") == std::string::npos);

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
