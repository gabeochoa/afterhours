#include <afterhours/ah.h>
#include <afterhours/src/drawing_helpers.h>

#include <cstdint>
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
    std::fprintf(stderr, "  FAIL: %s\n", what.c_str());
  }
}

static bool pixel_is(const RGBACapture &frame, int x, int y, uint8_t r,
                     uint8_t g, uint8_t b, uint8_t a) {
  const size_t offset =
      (static_cast<size_t>(y) * static_cast<size_t>(frame.width) +
       static_cast<size_t>(x)) *
      4;
  return frame.pixels[offset] == r && frame.pixels[offset + 1] == g &&
         frame.pixels[offset + 2] == b && frame.pixels[offset + 3] == a;
}

int main() {
  raylib::SetTraceLogLevel(raylib::LOG_ERROR);
  raylib::SetConfigFlags(raylib::FLAG_WINDOW_HIDDEN);
  raylib::InitWindow(64, 64, "texture_rgba_io_test");

  std::vector<uint8_t> original;
  for (int y = 0; y < 4; y++) {
    for (int x = 0; x < 4; x++) {
      original.push_back(static_cast<uint8_t>(10 + y * 40));
      original.push_back(static_cast<uint8_t>(20 + x * 10));
      original.push_back(30);
      original.push_back(255);
    }
  }

  TextureType texture = load_texture_from_rgba(original.data(), 4, 4);
  check(texture.id != 0, "texture uploads");

  auto unflipped = read_texture_rgba(texture);
  check(unflipped && unflipped->width == 4 && unflipped->height == 4,
        "unflipped readback returns 4x4");
  if (unflipped) {
    check(pixel_is(*unflipped, 0, 0, 10, 20, 30, 255),
          "unflipped row 0 is the original top row");
    check(pixel_is(*unflipped, 0, 3, 130, 20, 30, 255),
          "unflipped row 3 is the original bottom row");
  }

  auto flipped = read_texture_rgba(texture, std::nullopt, true);
  check(flipped && flipped->width == 4 && flipped->height == 4,
        "flipped readback returns 4x4");
  if (flipped) {
    check(pixel_is(*flipped, 0, 0, 130, 20, 30, 255),
          "flipped row 0 is the original bottom row");
    check(pixel_is(*flipped, 0, 3, 10, 20, 30, 255),
          "flipped row 3 is the original top row");
  }

  std::vector<uint8_t> patch(2 * 2 * 4);
  for (size_t i = 0; i < patch.size(); i += 4) {
    patch[i] = 200;
    patch[i + 1] = 201;
    patch[i + 2] = 202;
    patch[i + 3] = 255;
  }
  check(update_texture_rgba(texture, patch, RectangleType{1, 1, 2, 2}),
        "sub-rect update succeeds");

  auto after = read_texture_rgba(texture);
  check(after.has_value(), "readback after update succeeds");
  if (after) {
    check(pixel_is(*after, 1, 1, 200, 201, 202, 255),
          "updated pixel at (1,1) reads back exactly");
    check(pixel_is(*after, 2, 2, 200, 201, 202, 255),
          "updated pixel at (2,2) reads back exactly");
    check(pixel_is(*after, 0, 0, 10, 20, 30, 255),
          "pixel outside the sub-rect is unchanged");
    check(pixel_is(*after, 3, 3, 130, 50, 30, 255),
          "bottom-right pixel outside the sub-rect is unchanged");
  }

  auto sub = read_texture_rgba(texture, RectangleType{1, 1, 2, 2});
  check(sub && sub->width == 2 && sub->height == 2,
        "sub-rect readback returns 2x2");
  if (sub) {
    check(pixel_is(*sub, 0, 0, 200, 201, 202, 255),
          "sub-rect readback starts at the updated region");
    check(pixel_is(*sub, 1, 1, 200, 201, 202, 255),
          "sub-rect readback ends at the updated region");
  }

  check(!update_texture_rgba(TextureType{}, original),
        "update rejects an empty texture");
  check(!read_texture_rgba(TextureType{}), "read rejects an empty texture");
  check(!update_texture_rgba(texture, std::span<const uint8_t>(original).first(4)),
        "update rejects a truncated buffer");
  check(!update_texture_rgba(texture, patch, RectangleType{3, 3, 2, 2}),
        "update rejects an out-of-bounds rect");
  check(!read_texture_rgba(texture, RectangleType{3, 3, 2, 2}),
        "read rejects an out-of-bounds rect");

  unload_texture(texture);
  raylib::CloseWindow();
  std::printf("%d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
