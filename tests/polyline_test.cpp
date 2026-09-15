// Dashing and arc-length resampling, both geometry, so both testable exactly.
// The none backend records its draws, which is how the dash pattern is checked
// rather than eyeballed.

#define FMT_HEADER_ONLY
#include <fmt/format.h>

#include <afterhours/ah.h>
#include <afterhours/src/drawing_helpers.h>

#include <stdexcept>

namespace afterhours {
inline void bounded_draw_line(Vector2Type a, Vector2Type b, float thickness,
                              Color color) {
  if (capture::calls().size() >= 1000)
    throw std::runtime_error("dash draw budget exceeded");
  draw_line_ex(a, b, thickness, color);
}
} // namespace afterhours

#define draw_line_ex bounded_draw_line
#include <afterhours/src/polyline.h>
#undef draw_line_ex

#include <cmath>
#include <cstdio>

using namespace afterhours;

static int checks = 0;
static int failures = 0;
static void check(bool cond, const char *expr, int line) {
  checks++;
  if (!cond) {
    failures++;
    std::fprintf(stderr, "  FAIL: %s  (line %d)\n", expr, line);
  }
}
#define CHECK(expr) check((expr), #expr, __LINE__)
static bool near(float a, float b, float tol = 0.01f) {
  return std::fabs(a - b) < tol;
}

int main() {
  using polyline::distance;

  // A straight 100px line, dash 10 gap 10, is five dashes.
  {
    capture::clear();
    std::vector<Vector2Type> line{{0.f, 0.f}, {100.f, 0.f}};
    polyline::draw_dashed(line, 1.f, Color{255, 0, 0, 255}, 10.f, 10.f);
    CHECK(capture::calls().size() == 5);
  }

  // Phase shifts the pattern, which is what animates it.
  {
    capture::clear();
    std::vector<Vector2Type> line{{0.f, 0.f}, {100.f, 0.f}};
    polyline::draw_dashed(line, 1.f, Color{255, 0, 0, 255}, 10.f, 10.f, 10.f);
    // Starting in the gap drops the leading dash and adds a trailing one.
    CHECK(capture::calls().size() == 5);
  }

  // A dash that straddles a corner stays one dash: arc length accumulates
  // across vertices rather than restarting at each, which is what a
  // hand-rolled per-segment version gets wrong.
  {
    capture::clear();
    std::vector<Vector2Type> corner{{0.f, 0.f}, {5.f, 0.f}, {5.f, 5.f}};
    polyline::draw_dashed(corner, 1.f, Color{0, 255, 0, 255}, 10.f, 10.f);
    // 10px of line, one dash of 10, split by the corner into two draws.
    CHECK(capture::calls().size() == 2);
  }

  // Degenerate inputs draw nothing rather than looping forever.
  {
    capture::clear();
    std::vector<Vector2Type> one{{0.f, 0.f}};
    polyline::draw_dashed(one, 1.f, Color{}, 10.f, 10.f);
    std::vector<Vector2Type> line{{0.f, 0.f}, {10.f, 0.f}};
    polyline::draw_dashed(line, 1.f, Color{}, 0.f, 10.f);
    polyline::draw_dashed(line, 1.f, Color{}, -5.f, 10.f);
    CHECK(capture::calls().empty());
  }

  // Zero gap is a solid line, not an infinite loop.
  {
    capture::clear();
    std::vector<Vector2Type> line{{0.f, 0.f}, {100.f, 0.f}};
    polyline::draw_dashed(line, 1.f, Color{}, 10.f, 0.f);
    CHECK(capture::calls().size() == 10);
  }

  // Short curved cables can land just below a fractional dash boundary.
  // Stop excess draws in the test before a regression allocates millions.
  {
    capture::clear();
    const std::vector<Vector2Type> curve{
        {0.f, 0.f}, {6.39941406f, 0.584088445f},
        {10.7890625f, 0.743716359f}, {13.4560547f, 0.5250265f},
        {14.6875f, -0.025838837f}, {14.7705078f, -0.862736821f},
        {13.9921875f, -1.93952501f}, {12.6396484f, -3.21006107f},
        {11.f, -4.62820148f}, {9.36035156f, -6.14780521f},
        {8.0078125f, -7.72272825f}, {7.22949219f, -9.3068285f},
        {7.3125f, -10.8539639f}, {8.54394531f, -12.3179913f},
        {11.2109375f, -13.6527681f}, {15.6005859f, -14.8121519f},
        {22.f, -15.75f}};
    const float period = polyline::total_length(curve) / 2.5f;
    const float dash = period * .28f;
    try {
      polyline::draw_dashed(curve, 2.f, Color{}, dash, period - dash);
      CHECK(capture::calls().size() <= 20);
      float inked_length = 0.f;
      for (const auto &call : capture::calls())
        inked_length += std::hypot(call.rect.width, call.rect.height);
      CHECK(near(inked_length, 3.f * dash, .001f));
    } catch (const std::runtime_error &) {
      CHECK(false);
    }
  }

  // Resampling: evenly spaced by DISTANCE, which is the whole point. A
  // uniform-t sampler bunches where the curve is tight.
  {
    // Deliberately uneven input: a long leg then a short one.
    std::vector<Vector2Type> uneven{{0.f, 0.f}, {90.f, 0.f}, {100.f, 0.f}};
    auto even = polyline::resample_by_arclength(uneven, 10.f);
    CHECK(even.size() == 11);
    bool spaced = true;
    for (size_t i = 0; i + 1 < even.size(); i++)
      if (!near(distance(even[i], even[i + 1]), 10.f))
        spaced = false;
    CHECK(spaced);
  }

  // The end point survives whatever the spacing, or the curve stops short.
  {
    std::vector<Vector2Type> line{{0.f, 0.f}, {25.f, 0.f}};
    auto even = polyline::resample_by_arclength(line, 10.f);
    CHECK(near(even.back().x, 25.f));
    CHECK(near(even.front().x, 0.f));
  }

  // Spacing holds around a corner too, measured along the path.
  {
    std::vector<Vector2Type> corner{{0.f, 0.f}, {10.f, 0.f}, {10.f, 10.f}};
    auto even = polyline::resample_by_arclength(corner, 5.f);
    bool spaced = true;
    for (size_t i = 0; i + 1 < even.size(); i++)
      if (distance(even[i], even[i + 1]) > 5.01f)
        spaced = false;
    CHECK(spaced);
    CHECK(near(polyline::total_length(corner), 20.f));
  }

  // Degenerate resample returns the input rather than an empty curve.
  {
    std::vector<Vector2Type> line{{0.f, 0.f}, {10.f, 0.f}};
    CHECK(polyline::resample_by_arclength(line, 0.f).size() == 2);
    std::vector<Vector2Type> one{{1.f, 2.f}};
    CHECK(polyline::resample_by_arclength(one, 5.f).size() == 1);
  }

  capture::clear();
  std::printf("%d/%d checks passed\n", checks - failures, checks);
  if (failures == 0) std::printf("All checks passed!\n");
  return failures == 0 ? 0 : 1;
}
