#include <cassert>
#include <cstdio>

#include "../../../../ah.h"
#include "../../../../src/font_helper.h"

int main(int, char **) {
  const afterhours::Font font{};
  const float row_width = 120.f;

  for (const char *label : {"Play", "Settings and controls"}) {
    const float w = afterhours::measure_text(font, label, 16.f, 1.f).x;
    std::printf("%-24s estimated %5.1fpx, %s\n", label, static_cast<double>(w),
                w <= row_width ? "fits" : "overflows");
  }
  assert(afterhours::measure_text(font, "Settings and controls", 16.f, 1.f).x >
         row_width);

  afterhours::set_measure_text_fn([](const char *text, float size, float) {
    return Vector2Type{static_cast<float>(std::strlen(text)) * size * 0.6f,
                       size};
  });
  std::printf("with a measure fn: %.1fpx\n",
              static_cast<double>(
                  afterhours::measure_text(font, "Play", 16.f, 1.f).x));
  std::printf("headless_measure: ok\n");
  return 0;
}
