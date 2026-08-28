// Dashing and arc-length resampling, both geometry, so both testable exactly.
// The none backend records its draws, which is how the dash pattern is checked
// rather than eyeballed.

#define FMT_HEADER_ONLY
#include <fmt/format.h>

#include <afterhours/ah.h>
#include <afterhours/src/polyline.h>

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
