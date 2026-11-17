#define AFTER_HOURS_USE_NONE
#include <afterhours/src/developer.h>

#include <cstddef>
#include <cstdio>

static_assert(sizeof(Vector4Type) == 4 * sizeof(float));
static_assert(sizeof(MatrixType) == 16 * sizeof(float));
static_assert(offsetof(MatrixType, m4) == 1 * sizeof(float));
static_assert(offsetof(MatrixType, m1) == 4 * sizeof(float));
static_assert(offsetof(MatrixType, m15) == 15 * sizeof(float));

int main() {
  Vector4Type v{1, 2, 3, 4};
  MatrixType m{1, 0, 0, 5, 0, 1, 0, 6, 0, 0, 1, 7, 0, 0, 0, 1};
  bool ok = v.x == 1 && v.w == 4 && m.m12 == 5 && m.m13 == 6 && m.m14 == 7;
  printf("value types (none): %s\n", ok ? "ok" : "FAIL");
  return ok ? 0 : 1;
}
