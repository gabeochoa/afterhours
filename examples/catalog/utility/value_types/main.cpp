#include <cstdio>

#include "../../../../src/developer.h"

static Vector4Type transform(const MatrixType &m, Vector4Type v) {
  return {m.m0 * v.x + m.m4 * v.y + m.m8 * v.z + m.m12 * v.w,
          m.m1 * v.x + m.m5 * v.y + m.m9 * v.z + m.m13 * v.w,
          m.m2 * v.x + m.m6 * v.y + m.m10 * v.z + m.m14 * v.w,
          m.m3 * v.x + m.m7 * v.y + m.m11 * v.z + m.m15 * v.w};
}

int main() {
  MatrixType translate{1, 0, 0, 10, 0, 1, 0, 20, 0, 0, 1, 30, 0, 0, 0, 1};
  Vector4Type point{1, 2, 3, 1};
  Vector4Type moved = transform(translate, point);
  std::printf("(%g, %g, %g) -> (%g, %g, %g)\n", point.x, point.y, point.z,
              moved.x, moved.y, moved.z);
  return moved.x == 11 && moved.y == 22 && moved.z == 33 ? 0 : 1;
}
