#define FMT_HEADER_ONLY
// stb_image_write for encode_image/export_image: one TU defines the
// implementation, in every backend variant of this test.
#define AFTERHOURS_STB_IMAGE_IMPLEMENTATION
#if !defined(AFTER_HOURS_USE_RAYLIB) && !defined(AFTER_HOURS_USE_SOKOL)
#define AFTER_HOURS_USE_NONE
#include <fmt/format.h>
#include <afterhours/ah.h>
#else
#include <afterhours/src/graphics.h>
#endif
#include <afterhours/src/image.h>

#include <unistd.h>

#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
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

static bool same(Color a, Color b) {
  return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
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

static void check_cpu() {
  auto size = image_size(png_3x2);
  check(size && size->width == 3 && size->height == 2, "image_size reads 3x2");

  auto image = decode_image(png_3x2);
  check(image && image->width == 3 && image->height == 2 &&
            image->rgba.size() == 3 * 2 * 4,
        "decode_image returns 3x2 RGBA");
  if (image) {
    check(same(image->pixel(0, 0), Color{255, 0, 0, 255}), "pixel(0,0) red");
    check(same(image->pixel(2, 0), Color{0, 0, 255, 255}), "pixel(2,0) blue");
    check(same(image->pixel(0, 1), Color{10, 20, 30, 40}), "pixel(0,1) keeps alpha");
    check(same(image->pixel(2, 1), Color{1, 2, 3, 4}), "pixel(2,1) last texel");
  }

  check(!decode_image(png_3x2, {.max_dimension = 2}), "max_dimension rejects");
  check(!decode_image(png_3x2, {.max_bytes = 23}), "max_bytes rejects");
  check(decode_image(png_3x2, {.max_dimension = 3, .max_bytes = 24}).has_value(),
        "limits are inclusive");

  const std::array<std::uint8_t, 8> junk = {1, 2, 3, 4, 5, 6, 7, 8};
  check(!image_size(junk), "image_size rejects junk");
  check(!decode_image(junk), "decode_image rejects junk");
  auto truncated = std::span<const std::uint8_t>(png_3x2).first(40);
  check(!decode_image(truncated), "decode_image rejects a truncated file");

  const auto path = std::filesystem::temp_directory_path() /
                    ("afterhours_image_test_" + std::to_string(::getpid()) + ".png");
  {
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char *>(png_3x2.data()), png_3x2.size());
  }
  auto file_size = image_size(path);
  check(file_size && file_size->width == 3 && file_size->height == 2,
        "image_size(path) reads 3x2");
  auto loaded = load_image(path);
  check(loaded && image && loaded->rgba == image->rgba,
        "load_image matches decode_image");
  std::filesystem::remove(path);
  check(!image_size(path), "image_size(path) rejects a missing file");
  check(!load_image(path), "load_image rejects a missing file");

  auto made = make_image(2, 2, Color{9, 8, 7, 6});
  check(made && made->width == 2 && made->height == 2 &&
            made->rgba.size() == 16 &&
            same(made->pixel(1, 1), Color{9, 8, 7, 6}),
        "make_image fills every texel");
  check(!make_image(0, 2, Color{0, 0, 0, 0}), "make_image rejects zero width");
  check(!make_image(2, 2, Color{0, 0, 0, 0}, {.max_bytes = 15}),
        "make_image honors max_bytes");

  auto encoded = made ? encode_image(*made) : std::nullopt;
  check(encoded && image_size(*encoded)->width == 2, "encode_image writes a PNG");
  auto roundtrip = encoded ? decode_image(*encoded) : std::nullopt;
  check(roundtrip && made && roundtrip->rgba == made->rgba,
        "encode_image round-trips through decode_image");
  check(encoded && encoded->size() > 8 && (*encoded)[0] == 0x89 &&
            (*encoded)[1] == 0x50,
        "encode_image output starts with the PNG signature");
  check(!encode_image(Image{}), "encode_image rejects an empty image");

  auto export_path = std::filesystem::temp_directory_path() /
                     ("afterhours_image_export_test_" +
                      std::to_string(::getpid()) + ".png");
  check(made && export_image(*made, export_path), "export_image writes the file");
  auto reloaded = load_image(export_path);
  check(reloaded && made && reloaded->rgba == made->rgba,
        "export_image round-trips through load_image");
  std::filesystem::remove(export_path);
}

#ifndef AFTER_HOURS_USE_NONE
static void check_upload() {
  graphics::Config config;
  config.display = graphics::DisplayMode::Headless;
  config.width = 64;
  config.height = 32;
  check(graphics::init(config), "headless init");
  auto image = decode_image(png_3x2);
  check(image.has_value(), "decode for upload");
  if (!image)
    return;
  TextureType texture =
      load_texture_from_rgba(image->rgba.data(), image->width, image->height);
  check(texture.width == 3 && texture.height == 2, "texture is 3x2");

  auto &rt = graphics::get_render_texture();
  graphics::begin_frame();
#ifdef AFTER_HOURS_USE_RAYLIB
  graphics::clear_background(Color{0, 0, 0, 255});
#endif
  draw_texture_pro(texture, RectangleType{0, 0, 3, 2},
                   RectangleType{0, 0, 48, 32}, Vector2Type{0, 0}, 0,
                   Color{255, 255, 255, 255});
  graphics::end_frame();
  auto rgba = capture_render_texture_rgba(rt);
  check(rgba && rgba->width == 64, "readback");
  if (rgba) {
    auto dominant = [&](int x, int channel) {
      const auto *p = rgba->pixels.data() + (8 * rgba->width + x) * 4;
      for (int c = 0; c < 3; ++c)
        if ((c == channel) != (p[c] > 200))
          return false;
      return true;
    };
    check(dominant(8, 0), "drawn texel 0 is red");
    check(dominant(24, 1), "drawn texel 1 is green");
    check(dominant(40, 2), "drawn texel 2 is blue");
  }
  unload_texture(texture);
  graphics::shutdown();
}
#endif

int main() {
  check_cpu();
#ifndef AFTER_HOURS_USE_NONE
  check_upload();
#endif
  std::printf("image_test: %d/%d checks passed\n", checks - failures, checks);
  return failures > 0 ? 1 : 0;
}
