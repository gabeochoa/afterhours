#include <cassert>
#include <cstdio>
#include <vector>

#include "../../../../ah.h"
#include "../../../../src/font_helper.h"

int main(int, char **) {
  raylib::SetTraceLogLevel(raylib::LOG_WARNING);

  std::vector<unsigned char> embedded;
  assert(afterhours::font_coverage::read_file(FONT_PATH, embedded));

  const afterhours::Font font = afterhours::load_font_from_memory(
      embedded.data(), static_cast<int>(embedded.size()), "body");

  std::printf("loaded %d glyphs from %zu bytes in memory\n", font.glyphCount,
              embedded.size());
  std::printf("covers 'A': %s, covers U+2603: %s\n",
              afterhours::font_has_glyph(font, 'A') ? "yes" : "no",
              afterhours::font_has_glyph(font, 0x2603) ? "yes" : "no");
  assert(font.glyphCount > 0 && afterhours::font_has_glyph(font, 'A'));
  std::printf("embedded_font: ok\n");
  return 0;
}
