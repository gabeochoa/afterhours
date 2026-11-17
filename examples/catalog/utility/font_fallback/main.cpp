#include <cassert>
#include <cstdio>
#include <string>

#include "../../../../ah.h"
#include "../../../../src/font_helper.h"

int main(int, char **) {
  raylib::SetTraceLogLevel(raylib::LOG_WARNING);
  const std::string label = "Caf\xC3\xA9 \xC5\x8D \xE2\x98\x83";

  const afterhours::Font body =
      afterhours::load_font_for_string(label, FONT_DIR "/AtkinsonHyperlegible-Regular.ttf");
  const afterhours::Font narrow =
      afterhours::load_font_for_string(label + "x", FONT_DIR "/ArchivoNarrow-Regular.ttf");

  std::string storage;
  const char *drawn = afterhours::drawable_text(body, label.c_str(), storage);
  std::printf("\"%s\" draws as \"%s\"\n", label.c_str(), drawn);
  for (const afterhours::FaceRun &run : afterhours::fallback_runs(body, drawn))
    std::printf("  \"%s\" from %s\n", run.text.c_str(),
                run.face.glyphs == body.glyphs ? "Atkinson" : "Archivo");

  assert(afterhours::font_has_glyph(narrow, 0x014D));
  std::printf("font_fallback: ok\n");
  return 0;
}
