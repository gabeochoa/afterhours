#define AFTER_HOURS_EXAMPLE_GRAPHICS
#include "../../../example_helper.h"
#include "../../../../src/backends/sokol/drawing_helpers.h"
#include "../../../../src/core/key_codes.h"

#include <cstdio>

namespace g = afterhours::graphics;
using namespace afterhours;
using namespace afterhours::examples;

int main() {
  Graphics gfx(Graphics::headless(320, 180));
  if (!gfx.ok()) {
    std::printf("no GPU, nothing to show\n");
    return 0;
  }

  Font font = load_font_from_file(FONT_PATH);

  const unsigned char checker[16] = {255, 255, 255, 255, 0, 0, 0, 255,
                                     0,   0,   0,   255, 255, 255, 255, 255};
  TextureType tile =
      metal_texture_detail::load_texture_from_pixels(checker, 2, 2);
  set_texture_wrap(tile, TEXTURE_WRAP_REPEAT);
  set_texture_filter(tile, TEXTURE_FILTER_TRILINEAR);

  for (int frame = 0; frame < 3; ++frame) {
    for (int key = g::get_key_pressed(); key != keys::NULL_KEY;
         key = g::get_key_pressed())
      std::printf("key %d\n", key);

    g::begin_drawing();
    draw_fps(10, 10);
    g::end_drawing();
  }

  unload_texture(tile);
  unload_font(font);
  std::printf("drew 3 frames with draw_fps\n");
  return 0;
}
