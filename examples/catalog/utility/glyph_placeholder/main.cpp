#include <cassert>
#include <cstdio>
#include <string>

#include "../../../../ah.h"
#include "../../../../src/font_helper.h"

int main(int, char **) {
  raylib::SetTraceLogLevel(raylib::LOG_WARNING);
  const char *labels[] = {
      "Push \xE2\x86\x91",
      "Snow \xE2\x98\x83",
      "Family \xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA7",
      "Caf\xC3\xA9",
  };
  std::string every_label;
  for (const char *label : labels)
    every_label += label;
  const afterhours::Font font =
      afterhours::load_font_for_string(every_label, FONT_PATH);

  for (const char *label : labels) {
    std::string storage;
    std::printf("%-24s draws as \"%s\"\n", label,
                afterhours::drawable_text(font, label, storage));
  }

  std::string storage;
  assert(std::string(afterhours::drawable_text(font, "Snow \xE2\x98\x83",
                                               storage)) == "Snow ?");
  std::printf("glyph_placeholder: ok\n");
  return 0;
}
