#include <cassert>
#include <cstdio>

#include "../../../../ah.h"
#include "../../../../src/graphics.h"
#include "../../../../src/plugins/ui/ui_core_components.h"

using namespace afterhours;

int main(int, char **argv) {
  graphics::Config config;
  config.display = graphics::DisplayMode::Headless;
  config.width = 64;
  config.height = 64;
  if (!graphics::init(config))
    return 2;

  const char *font_file = "../../../../tests/fixtures/"
                          "AtkinsonHyperlegible-Regular.ttf";
  if (argv[1])
    font_file = argv[1];

  int codepoints[] = {'A', 'B', 0xE9};

  ui::FontManager fonts;
  fonts.load_font_with_codepoints("display", font_file, codepoints, 3, 64);
  fonts.load_font_with_codepoints("body", font_file, codepoints, 3);

  const int display_size = fonts.get_font("display").baseSize;
  const int body_size = fonts.get_font("body").baseSize;
  std::printf("display loaded at %dpx, body at the default %dpx\n",
              display_size, body_size);
  assert(display_size == 64);
  assert(body_size == 32);

  graphics::shutdown();
  return 0;
}
