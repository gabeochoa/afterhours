#include <cassert>
#include <cmath>
#include <cstdio>

#include "../../../../ah.h"
#include "../../../../src/font_coverage.h"

int main(int, char **) {
  const auto per_point = afterhours::font_coverage::font_size_per_point(FONT_PATH);
  assert(per_point.has_value());

  struct Token {
    const char *role;
    float points;
  };
  const Token tokens[] = {{"caption", 11.f}, {"body", 13.f}, {"title", 20.f}};
  for (const Token &t : tokens)
    std::printf("%-8s %4.1fpt -> font size %5.2f\n", t.role,
                static_cast<double>(t.points),
                static_cast<double>(t.points * *per_point));

  assert(std::fabs(13.f * *per_point - 16.12f) < 0.01f);
  std::printf("font_points: ok (%.3f font size per point)\n",
              static_cast<double>(*per_point));
  return 0;
}
