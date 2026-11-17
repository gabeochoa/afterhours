#include <afterhours/src/backends/raylib/font_helper.h>

#include <cstdio>
#include <filesystem>
#include <string>

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

static std::string fixture(const char *name) {
  return (std::filesystem::path(AFTERHOURS_TEST_FIXTURES) / name).string();
}

int main() {
  raylib::SetTraceLogLevel(raylib::LOG_WARNING);
  namespace fc = afterhours::font_coverage;
  const std::string atkinson_path = fixture("AtkinsonHyperlegible-Regular.ttf");
  const std::string archivo_path = fixture("ArchivoNarrow-Regular.ttf");
  fc::Ranges atkinson_cmap, archivo_cmap;
  check(fc::parse_file(atkinson_path.c_str(), atkinson_cmap) &&
            fc::parse_file(archivo_path.c_str(), archivo_cmap),
        "fixture cmaps parse");
  int cp = 0;
  for (int c = 0x80; c < 0x2000 && !cp; c++)
    if (!fc::covers(atkinson_cmap, static_cast<uint32_t>(c)) &&
        fc::covers(archivo_cmap, static_cast<uint32_t>(c)))
      cp = c;
  check(cp != 0, "a codepoint only Archivo covers exists");
  int len = 0;
  const std::string only_archivo = raylib::CodepointToUTF8(cp, &len);
  const std::string text = "A" + only_archivo.substr(0, static_cast<size_t>(len)) + "B";

  raylib::Font atkinson = afterhours::load_font_for_string(text + "C", atkinson_path);
  const raylib::Font archivo = afterhours::load_font_for_string(text, archivo_path);
  check(!afterhours::font_has_glyph(atkinson, cp) &&
            afterhours::font_has_glyph(archivo, cp),
        "the primary lacks the codepoint and the second face has it");

  std::string storage;
  check(std::string(afterhours::drawable_text(atkinson, text.c_str(), storage)) == text,
        "a codepoint another loaded face covers is not replaced");

  const auto runs = afterhours::fallback_runs(atkinson, text.c_str());
  check(runs.size() == 3, "text splits into primary, fallback, primary runs");
  if (runs.size() == 3) {
    check(runs[0].face.glyphs == atkinson.glyphs && runs[0].text == "A",
          "first run is the primary");
    check(runs[1].face.glyphs == archivo.glyphs, "middle run is the fallback face");
    check(runs[2].face.glyphs == atkinson.glyphs && runs[2].text == "B",
          "last run is the primary again");
  }
  check(afterhours::fallback_runs(atkinson, "plain").empty(),
        "text the primary covers takes the native path");

  atkinson.texture.id = 1;
  afterhours::record_font_coverage(atkinson, atkinson_path.c_str());
  const float size = 32.f, spacing = 1.f;
  const auto width = [&](const raylib::Font f, const std::string &s) {
    return raylib::MeasureTextEx(f, s.c_str(), size, spacing).x;
  };
  const float expected = width(atkinson, "A") + spacing +
                         width(archivo, runs.size() == 3 ? runs[1].text : "") +
                         spacing + width(atkinson, "B");
  const float measured = afterhours::measure_text(atkinson, text.c_str(), size, spacing).x;
  check(std::fabs(measured - expected) < 0.01f,
        "measure_text sums the runs the way draw_text_ex places them");

  printf("%d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
