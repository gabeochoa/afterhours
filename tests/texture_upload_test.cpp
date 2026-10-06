// texture_upload_test.cpp
// Decode on the CPU, then load_texture_from_rgba on the render thread.
// Built three ways: no backend (always fails), raylib, and sokol/Metal.

#define FMT_HEADER_ONLY
#if defined(SOKOL_METAL) && !defined(AFTER_HOURS_USE_METAL)
#define AFTER_HOURS_USE_METAL
#endif
#include <afterhours/src/plugins/image_decode.h>
#if defined(AFTER_HOURS_USE_RAYLIB) || defined(AFTER_HOURS_USE_METAL)
#include <afterhours/src/graphics.h>
#endif
#include <afterhours/src/drawing_helpers.h>

#include <cstdio>
#include <string>
#include <vector>

using namespace afterhours;

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

// 2x2 uncompressed TGA, top-left origin: red, green / blue, white.
static std::vector<unsigned char> quad_tga() {
  std::vector<unsigned char> out(18, 0);
  out[2] = 2;
  out[12] = 2;
  out[14] = 2;
  out[16] = 32;
  out[17] = 0x28;
  const unsigned char bgra[] = {0, 0, 255, 255, 0, 255, 0, 255, 255, 0, 0, 255, 255, 255, 255, 255};
  out.insert(out.end(), std::begin(bgra), std::end(bgra));
  return out;
}

int main() {
  printf("=== texture upload tests ===\n\n");
  image_decode::Budget budget(1 << 20);
  {
    auto image = image_decode::decode(quad_tga(), {}, {}, &budget);
    check(image && image->width() == 2 && image->height() == 2, "fixture decodes");
    check(!load_texture_from_rgba(nullptr, 2, 2), "null pixels fail");
    check(!load_texture_from_rgba(image->pixels(), 0, 2), "zero width fails");
    check(!load_texture_from_rgba(image->pixels(), 2, -1), "negative height fails");
#if defined(AFTER_HOURS_BACKEND_NONE)
    check(!load_texture_from_rgba(image->pixels(), 2, 2), "the none backend reports failure");
#elif defined(AFTER_HOURS_USE_METAL)
    check(!load_texture_from_rgba(image->pixels(), 2, 2), "fails before graphics::init");
#endif
  }
  check(budget.reserved() == 0, "a failed upload leaves the caller's image to release");

#if defined(AFTER_HOURS_USE_RAYLIB) || defined(AFTER_HOURS_USE_METAL)
  graphics::Config config;
  config.display = graphics::DisplayMode::Headless;
  config.width = 64;
  config.height = 32;
  check(graphics::init(config), "headless init");
  auto &rt = graphics::get_render_texture();
  auto image = image_decode::decode(quad_tga(), {}, {}, &budget);
  auto texture = load_texture_from_rgba(image->pixels(), image->width(), image->height(),
                                        TEXTURE_FILTER_BILINEAR);
  check(texture.has_value(), "upload succeeds on the render thread");
  check(texture && texture->width == 2 && texture->height == 2, "texture has the image size");
  image = tl::unexpected(image_decode::Error{image_decode::ErrorCode::Cancelled, "released"});
  check(budget.reserved() == 0, "pixels can be released right after upload");
  if (texture) {
    graphics::begin_frame();
    graphics::clear_background(Color{0, 0, 0, 255});
    draw_texture_pro(*texture, RectangleType{0, 0, 2, 2}, RectangleType{0, 0, 64, 32},
                     Vector2Type{0, 0}, 0, Color{255, 255, 255, 255});
    graphics::end_frame();
    auto rgba = capture_render_texture_rgba(rt);
    check(rgba && rgba->width == 64 && rgba->height == 32, "captured the target");
    auto at = [&](int x, int y, int r, int g, int b) {
      const auto *p = rgba->pixels.data() + (static_cast<size_t>(y) * 64 + x) * 4;
      // Bilinear filtering blends a little across the texel boundary.
      auto near = [](int got, int want) { return got >= want - 24 && got <= want + 24; };
      return near(p[0], r) && near(p[1], g) && near(p[2], b) && p[3] == 255;
    };
    if (rgba) {
      check(at(16, 8, 255, 0, 0), "top-left texel is red");
      check(at(48, 8, 0, 255, 0), "top-right texel is green");
      check(at(16, 24, 0, 0, 255), "bottom-left texel is blue");
      check(at(48, 24, 255, 255, 255), "bottom-right texel is white");
    }
    auto trilinear = load_texture_from_rgba(std::vector<unsigned char>(16 * 16 * 4, 200).data(),
                                            16, 16, TEXTURE_FILTER_TRILINEAR);
    check(trilinear.has_value(), "trilinear upload succeeds");
    if (trilinear) unload_texture(*trilinear);
    unload_texture(*texture);
  }
  graphics::shutdown();
#endif

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
