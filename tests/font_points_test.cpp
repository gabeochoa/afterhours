#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#define STB_TRUETYPE_IMPLEMENTATION
#include <fontstash/stb_truetype.h>
#include <afterhours/src/font_coverage.h>

namespace fc = afterhours::font_coverage;

static int failures = 0;
#define CHECK(condition) do { if (!(condition)) { std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); ++failures; } } while (0)

static std::string fixture(const char *name) {
  return (std::filesystem::path(AFTERHOURS_TEST_FIXTURES) / name).string();
}

static void em_size_matches_stb(const char *name) {
  const std::string path = fixture(name);
  const auto per_point = fc::font_size_per_point(path.c_str());
  CHECK(per_point.has_value());
  if (!per_point)
    return;
  std::vector<unsigned char> bytes;
  CHECK(fc::read_file(path.c_str(), bytes));
  stbtt_fontinfo info;
  CHECK(stbtt_InitFont(&info, bytes.data(), 0));
  for (float points : {9.f, 13.f, 16.f, 31.5f}) {
    const float as_font_size = stbtt_ScaleForPixelHeight(&info, points * *per_point);
    const float as_em = stbtt_ScaleForMappingEmToPixels(&info, points);
    CHECK(std::fabs(as_font_size - as_em) < 1e-6f);
  }
  std::printf("%s: font size per point %.4f\n", name, static_cast<double>(*per_point));
}

int main() {
  em_size_matches_stb("AtkinsonHyperlegible-Regular.ttf");
  em_size_matches_stb("ArchivoNarrow-Regular.ttf");

  const auto atkinson = fc::font_size_per_point(fixture("AtkinsonHyperlegible-Regular.ttf").c_str());
  CHECK(atkinson && std::fabs(13.f * *atkinson - 16.12f) < 0.01f);

  CHECK(!fc::font_size_per_point("/nonexistent/font.ttf"));
  const unsigned char junk[64] = {};
  CHECK(!fc::font_size_per_point(junk, sizeof(junk)));

  if (failures) {
    std::fprintf(stderr, "%d failure(s)\n", failures);
    return 1;
  }
  std::printf("font_points_test: all checks passed\n");
  return 0;
}
