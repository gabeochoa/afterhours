// NONE has no rasterizer, so its half of the winding contract is the draw
// record: both vertex orders must reach capture::record, the same evidence
// raylib and sokol backends write before they hand off to the GPU.
#define AFTER_HOURS_USE_NONE
#include <afterhours/src/drawing_helpers.h>

#include <cassert>
#include <cstdio>
#include <string>

int main() {
  using namespace afterhours;
  capture::enable();

  capture::clear();
  draw_triangle(Vector2Type{4, 4}, Vector2Type{60, 4}, Vector2Type{32, 28},
                Color{255, 0, 0, 255});
  int cw = 0;
  for (const auto &c : capture::calls()) {
    if (c.op == "triangle") {
      cw++;
    }
  }
  assert(cw == 1);

  capture::clear();
  draw_triangle(Vector2Type{4, 4}, Vector2Type{32, 28}, Vector2Type{60, 4},
                Color{255, 0, 0, 255});
  int ccw = 0;
  for (const auto &c : capture::calls()) {
    if (c.op == "triangle") {
      ccw++;
    }
  }
  assert(ccw == 1);

  printf("triangle_winding_none_test: ok (cw=1 record, ccw=1 record)\n");
  return 0;
}
