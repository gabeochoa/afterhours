#include <afterhours/src/developer.h>
#include <afterhours/src/graphics.h>

#include <cstdio>
#include <type_traits>

static_assert(std::is_same_v<Vector4Type, raylib::Vector4>);
static_assert(std::is_same_v<MatrixType, raylib::Matrix>);
static_assert(std::is_same_v<afterhours::Vector4Type, raylib::Vector4>);
static_assert(std::is_same_v<afterhours::MatrixType, raylib::Matrix>);
static_assert(std::is_same_v<afterhours::graphics::Camera2D, raylib::Camera2D>);

int main() {
  raylib::Matrix ident = raylib::Matrix{1, 0, 0, 0, 0, 1, 0, 0,
                                        0, 0, 1, 0, 0, 0, 0, 1};
  MatrixType m = ident;
  Vector4Type v{1, 2, 3, 4};
  afterhours::graphics::Camera2D cam{};
  cam.zoom = 1.f;
  bool ok = m.m0 == 1 && m.m15 == 1 && v.w == 4 && cam.zoom == 1.f;
  printf("value types (raylib): %s\n", ok ? "ok" : "FAIL");
  return ok ? 0 : 1;
}
