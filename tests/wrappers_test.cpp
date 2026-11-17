#include <afterhours/src/graphics.h>
#include <afterhours/src/backends/raylib/drawing_helpers.h>
#include <afterhours/src/core/key_codes.h>

#include <cstdio>

static_assert(afterhours::keys::NULL_KEY == raylib::KEY_NULL);
static_assert(afterhours::TEXTURE_WRAP_REPEAT == raylib::TEXTURE_WRAP_REPEAT);
static_assert(afterhours::TEXTURE_WRAP_CLAMP == raylib::TEXTURE_WRAP_CLAMP);
static_assert(afterhours::TEXTURE_WRAP_MIRROR_REPEAT ==
              raylib::TEXTURE_WRAP_MIRROR_REPEAT);

int main() {
  int (*key_pressed)() = afterhours::graphics::get_key_pressed;
  void (*fps)(int, int) = afterhours::draw_fps;
  void (*wrap)(TextureType &, int) = afterhours::set_texture_wrap;
  void (*unload)(raylib::Font &) = afterhours::unload_font;

  TextureType empty{};
  afterhours::set_texture_wrap(empty, afterhours::TEXTURE_WRAP_REPEAT);

  bool ok = key_pressed && fps && wrap && unload;
  printf("raylib wrappers: %s\n", ok ? "ok" : "FAIL");
  return ok ? 0 : 1;
}
