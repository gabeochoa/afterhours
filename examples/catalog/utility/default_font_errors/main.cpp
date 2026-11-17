#include <cassert>
#include <cstdio>

#include "../../../../ah.h"
#include "../../../../src/drawing_helpers.h"

int main(int, char **) {
  raylib::SetTraceLogLevel(raylib::LOG_WARNING);
  const afterhours::Font body =
      afterhours::load_font_for_string("Score 0123456789", FONT_PATH);
  afterhours::set_default_font(body);

  const afterhours::Font fallback =
      afterhours::load_font_for_string("", "fonts/missing.ttf");
  std::printf("failed load returned the app's default font: %s\n",
              fallback.glyphs == body.glyphs ? "yes" : "no");
  assert(fallback.glyphs == body.glyphs);

  afterhours::clear_default_font();
  std::printf("default_font_errors: ok\n");
  return 0;
}
