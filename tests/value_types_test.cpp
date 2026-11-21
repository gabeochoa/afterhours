#include <afterhours/src/developer.h>
#include <afterhours/src/graphics.h>

#include <cstdio>
#include <type_traits>

static_assert(std::is_same_v<Vector4Type, raylib::Vector4>);
static_assert(std::is_same_v<MatrixType, raylib::Matrix>);
static_assert(std::is_same_v<afterhours::Vector4Type, raylib::Vector4>);
static_assert(std::is_same_v<afterhours::MatrixType, raylib::Matrix>);
static_assert(std::is_same_v<afterhours::graphics::Camera2D, raylib::Camera2D>);

static_assert(afterhours::distance_sq(Vector2Type{0, 0},
                                      Vector2Type{3, 4}) == 25.f);
static_assert(::distance_sq(Vector2Type{1, 1}, Vector2Type{1, 1}) == 0.f);

int main() {
  raylib::Matrix ident = raylib::Matrix{1, 0, 0, 0, 0, 1, 0, 0,
                                        0, 0, 1, 0, 0, 0, 0, 1};
  MatrixType m = ident;
  Vector4Type v{1, 2, 3, 4};
  afterhours::graphics::Camera2D cam{};
  cam.zoom = 1.f;
  float dsq = afterhours::distance_sq(Vector2Type{1, 1}, Vector2Type{4, 5});
  bool ok = m.m0 == 1 && m.m15 == 1 && v.w == 4 && cam.zoom == 1.f &&
            dsq == 25.f;
  printf("value types (raylib): %s\n", ok ? "ok" : "FAIL");
  return ok ? 0 : 1;
}
