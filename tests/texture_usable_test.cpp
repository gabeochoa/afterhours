#define FMT_HEADER_ONLY
#if !defined(AFTER_HOURS_USE_RAYLIB) && !defined(AFTER_HOURS_USE_SOKOL)
#define AFTER_HOURS_USE_NONE
#define AFTERHOURS_STB_IMAGE_IMPLEMENTATION
#include <fmt/format.h>
#include <afterhours/ah.h>
#else
#include <afterhours/src/graphics.h>
#endif
#include <afterhours/src/image.h>

#include <array>
#include <cstdio>
#include <string>

using namespace afterhours;

static int checks = 0;
static int failures = 0;

static void check(bool cond, const std::string &what) {
  ++checks;
  if (!cond) {
    ++failures;
    std::printf("  FAIL: %s\n", what.c_str());
  }
}

static const std::array<std::uint8_t, 86> png_3x2 = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
    0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x02,
    0x08, 0x06, 0x00, 0x00, 0x00, 0x9d, 0x74, 0x66, 0x1a, 0x00, 0x00, 0x00,
    0x1d, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x63, 0xf8, 0xcf, 0xc0, 0xf0,
    0x1f, 0x0c, 0x19, 0xfe, 0xff, 0x67, 0xe0, 0x12, 0x91, 0xd3, 0xf8, 0x0f,
    0xa4, 0x19, 0x99, 0x98, 0x59, 0x00, 0x8a, 0x8f, 0x09, 0x66, 0x56, 0x4f,
    0xe5, 0xee, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42,
    0x60, 0x82};

static void check_size_query() {
  auto size = image_size(png_3x2);
  check(size && size->width == 3 && size->height == 2, "image_size reads 3x2");
  check(!texture_is_usable(),
        "size query works with no usable texture (no context yet)");
}

#ifndef AFTER_HOURS_USE_NONE
static void check_live_backend() {
  graphics::Config config;
  config.display = graphics::DisplayMode::Headless;
  config.width = 64;
  config.height = 32;
  check(graphics::init(config), "headless init");
  check(texture_is_usable(), "usable after headless init");

  auto image = decode_image(png_3x2);
  check(image.has_value(), "decode for upload");
  if (image) {
    TextureType texture =
        load_texture_from_rgba(image->rgba.data(), image->width, image->height);
    check(texture.width == 3 && texture.height == 2,
          "upload matches usable answer");
    unload_texture(texture);
  }
  graphics::shutdown();
  check(!texture_is_usable(), "not usable after shutdown");
}
#endif

int main() {
#ifdef AFTER_HOURS_USE_NONE
  check(!texture_is_usable(), "none backend never creates textures");
#endif
  check_size_query();
#ifndef AFTER_HOURS_USE_NONE
  check_live_backend();
#endif
  std::printf("texture_usable_test: %d/%d checks passed\n", checks - failures,
              checks);
  return failures > 0 ? 1 : 0;
}
