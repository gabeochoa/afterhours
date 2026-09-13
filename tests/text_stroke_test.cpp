#include "ui_test_harness.h"
#include <afterhours/src/plugins/ui/text_stroke.h>
#include <algorithm>
#include <cmath>
#include <limits>

using afterhours::ui::text_stroke::dilate_alpha;

TEST(stroke_fills_between_thin_glyph_and_wide_outline) {
  constexpr int side = 129;
  std::vector<unsigned char> mask(side * side, 0);
  for (int y = 48; y <= 80; ++y) mask[y * side + 64] = 255;
  const auto result = dilate_alpha(mask, side, side, 32);
  for (int x = 33; x <= 95; ++x) CHECK(result[64 * side + x] == 255);
  CHECK(result[64 * side + 32] > 100);
  CHECK(result[64 * side + 31] == 0);
  CHECK(result[24 * side + 40] == 0);
}

TEST(stroke_is_round_not_square) {
  std::vector<unsigned char> mask(121, 0);
  mask[5 * 11 + 5] = 255;
  const auto result = dilate_alpha(mask, 11, 11, 3);
  CHECK(result[5 * 11 + 8] > 0);
  CHECK(result[8 * 11 + 8] == 0);
  CHECK(result[7 * 11 + 7] > 0);
}

TEST(stroke_distance_matches_direct_reference) {
  constexpr int width = 17, height = 13;
  std::vector<unsigned char> mask(width * height, 0);
  mask[3 * width + 4] = 255;
  mask[8 * width + 12] = 128;
  mask[10 * width + 7] = 210;
  for (float radius : {.25f, 1.f, 2.5f, 5.f}) {
    const auto result = dilate_alpha(mask, width, height, radius);
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        float distance = std::numeric_limits<float>::max();
        for (int sy = 0; sy < height; ++sy) {
          for (int sx = 0; sx < width; ++sx) {
            const auto value = mask[sy * width + sx];
            if (value == 0) continue;
            const float edge = 1.f - static_cast<float>(value) / 255.f;
            distance = std::min(distance, static_cast<float>((x - sx) * (x - sx) +
                (y - sy) * (y - sy)) + edge * edge);
          }
        }
        const int expected = static_cast<int>(255.f * std::clamp(radius + .5f - std::sqrt(distance), 0.f, 1.f));
        CHECK(std::abs(static_cast<int>(result[y * width + x]) - expected) <= 1);
      }
    }
  }
}

TEST(stroke_empty_and_zero_radius_are_stable) {
  const std::vector<unsigned char> empty(63, 0);
  CHECK(dilate_alpha(empty, 9, 7, 4) == empty);
  auto mask = empty;
  mask[31] = 123;
  CHECK(dilate_alpha(mask, 9, 7, 0) == mask);
  CHECK(dilate_alpha(mask, 9, 7, -1) == mask);
  CHECK(dilate_alpha(mask, 8, 7, 2).empty());
}

int main() { return ui_test::run_registered_tests("text stroke"); }
