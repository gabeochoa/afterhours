#define AFTER_HOURS_USE_NONE
#include <afterhours/src/developer.h>

#include <cstddef>
#include <cstdio>

static_assert(sizeof(Vector4Type) == 4 * sizeof(float));
static_assert(sizeof(MatrixType) == 16 * sizeof(float));
static_assert(offsetof(MatrixType, m4) == 1 * sizeof(float));
static_assert(offsetof(MatrixType, m1) == 4 * sizeof(float));
static_assert(offsetof(MatrixType, m15) == 15 * sizeof(float));

static_assert(afterhours::distance_sq(Vector2Type{0, 0},
                                      Vector2Type{3, 4}) == 25.f);
static_assert(::distance_sq(Vector2Type{1, 1}, Vector2Type{1, 1}) == 0.f);

int main() {
  Vector4Type v{1, 2, 3, 4};
  MatrixType m{1, 0, 0, 5, 0, 1, 0, 6, 0, 0, 1, 7, 0, 0, 0, 1};
  float dsq = afterhours::distance_sq(Vector2Type{1, 1}, Vector2Type{4, 5});
  bool ok = v.x == 1 && v.w == 4 && m.m12 == 5 && m.m13 == 6 && m.m14 == 7 &&
            dsq == 25.f;
  printf("value types (none): %s\n", ok ? "ok" : "FAIL");
  return ok ? 0 : 1;
}
