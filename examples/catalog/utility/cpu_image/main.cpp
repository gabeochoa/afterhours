#include <cstdio>

#define AFTER_HOURS_EXAMPLE_GRAPHICS
#include "../../../example_helper.h"
#include "../../../../src/image.h"

using namespace afterhours;
using namespace afterhours::examples;

int main(int, char **) {
  auto size = image_size(IMAGE_PATH);
  if (!size)
    return 1;
  std::printf("header says %dx%d\n", size->width, size->height);

  if (!load_image(IMAGE_PATH, {.max_dimension = 256}))
    std::printf("max_dimension 256 refuses it before decoding\n");

  auto image = load_image(IMAGE_PATH, {.max_dimension = 1024});
  if (!image)
    return 1;
  Color middle = image->pixel(256, 256);
  std::printf("pixel(256, 256) = %d %d %d %d\n", middle.r, middle.g, middle.b,
              middle.a);

  Graphics gfx(Graphics::headless(64, 64));
  if (!gfx.ok())
    return 1;

  TextureType texture =
      load_texture_from_rgba(image->rgba.data(), image->width, image->height);
  std::printf("uploaded a %dx%d texture\n", static_cast<int>(texture.width),
              static_cast<int>(texture.height));

  graphics::begin_frame();
  draw_texture_pro(texture,
                   RectangleType{0, 0, static_cast<float>(image->width),
                                 static_cast<float>(image->height)},
                   RectangleType{0, 0, 64, 64}, Vector2Type{0, 0}, 0,
                   Color{255, 255, 255, 255});
  graphics::end_frame();
  auto frame = capture_render_texture_rgba(graphics::get_render_texture());
  if (frame)
    std::printf("drew it into a %dx%d frame\n", frame->width, frame->height);

  unload_texture(texture);
  return frame ? 0 : 1;
}
