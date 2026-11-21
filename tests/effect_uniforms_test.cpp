// effect_uniforms_test.cpp
// R-118: Effect vec3/vec4/int/matrix uniforms + vertex shader, and matrix compose.
// Pixel checks read back a render texture; matrix checks compare to raymath.

#include <cmath>
#include <cstdio>
#include <random>
#include <string>

namespace raylib {
#include <raylib.h>
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#include <raymath.h>
#pragma clang diagnostic pop
} // namespace raylib

#include <afterhours/ah.h>
#include <afterhours/src/plugins/effects.h>
#include <afterhours/src/plugins/transform.h>

using namespace afterhours;

static int checks_run = 0, checks_passed = 0;
static void check(bool cond, const std::string &what) {
  checks_run++;
  if (cond) checks_passed++; else fprintf(stderr, "  FAIL: %s\n", what.c_str());
}
static float max_diff(const MatrixType &a, const raylib::Matrix &b) {
  const float *pa = &a.m0, *pb = &b.m0;
  float w = 0;
  for (int i = 0; i < 16; i++) w = std::fmax(w, std::fabs(pa[i] - pb[i]));
  return w;
}
static bool near(float a, float b) { return std::fabs(a - b) < 1e-4f; }

int main() {
  // Matrix math (no window needed).
  {
    std::mt19937 rng(7);
    std::uniform_real_distribution<float> u(-2.f, 2.f);
    float worst_mul = 0, worst_compose = 0;
    for (int i = 0; i < 200; i++) {
      float av[16], bv[16];
      for (float &x : av) x = u(rng);
      for (float &x : bv) x = u(rng);
      // MatrixType/raylib::Matrix share field order, so a flat copy is exact.
      MatrixType a, b;
      std::memcpy(&a, av, sizeof(av)); std::memcpy(&b, bv, sizeof(bv));
      raylib::Matrix ra, rb;
      std::memcpy(&ra, av, sizeof(av)); std::memcpy(&rb, bv, sizeof(bv));
      worst_mul = std::fmax(worst_mul, max_diff(matrix_multiply(a, b), raylib::MatrixMultiply(ra, rb)));

      Vector3Type pos{u(rng) * 5, u(rng) * 5, u(rng) * 5};
      Vector3Type e{u(rng) * 90, u(rng) * 90, u(rng) * 90};
      Vector3Type sc{1 + std::fabs(u(rng)), 1 + std::fabs(u(rng)), 1 + std::fabs(u(rng))};
      // raymath MatrixMultiply(a, b) applies a first, so build in application order.
      raylib::Matrix expected = raylib::MatrixMultiply(
          raylib::MatrixScale(sc.x, sc.y, sc.z),
          raylib::MatrixMultiply(
              raylib::MatrixRotateZ(e.z * DEG2RAD),
              raylib::MatrixMultiply(raylib::MatrixRotateX(e.x * DEG2RAD),
                                     raylib::MatrixMultiply(raylib::MatrixRotateY(e.y * DEG2RAD),
                                                            raylib::MatrixTranslate(pos.x, pos.y, pos.z)))));
      worst_compose = std::fmax(worst_compose, max_diff(matrix_compose(pos, e, sc), expected));
    }
    check(worst_mul < 1e-4f, "matrix_multiply matches raymath MatrixMultiply");
    check(worst_compose < 1e-4f, "matrix_compose matches scale, Z/X/Y rotations, translation");

    MatrixType parent = matrix_compose({10, 0, 0}, {0, 0, 0}, {1, 1, 1});
    MatrixType local = matrix_compose({0, 2, 0}, {0, 0, 0}, {2, 2, 2});
    Vector3Type p = transform_point(matrix_multiply(local, parent), {1, 1, 1});
    check(near(p.x, 12) && near(p.y, 4) && near(p.z, 2), "local then parent transforms a point");
    Vector3Type q = transform_point(matrix_identity(), {3, 4, 5});
    check(near(q.x, 3) && near(q.y, 4) && near(q.z, 5), "identity leaves a point alone");
  }

  raylib::SetTraceLogLevel(raylib::LOG_ERROR);
  raylib::SetConfigFlags(raylib::FLAG_WINDOW_HIDDEN);
  raylib::InitWindow(64, 64, "effect_uniforms_test");

  const char *fs = R"(#version 330
in vec2 fragTexCoord; in vec4 fragColor;
uniform vec3 u_vec3; uniform vec4 u_vec4; uniform float u_float; uniform int u_int; uniform mat4 u_mat;
out vec4 finalColor;
void main() {
  vec3 c = u_vec3 * u_float + u_vec4.rgb + vec3(u_mat[3][0], u_mat[3][1], u_mat[3][2]);
  finalColor = vec4(c, 1.0) * (u_int == 1 ? 1.0 : 0.0);
})";
  const char *vs = R"(#version 330
in vec3 vertexPosition; in vec2 vertexTexCoord; in vec4 vertexColor;
uniform mat4 mvp; uniform vec3 u_offset;
out vec2 fragTexCoord; out vec4 fragColor;
void main() { fragTexCoord = vertexTexCoord; fragColor = vertexColor;
  gl_Position = mvp * vec4(vertexPosition + u_offset, 1.0); })";

  auto render_pixel = [&](effects::Effect &e, int x, int y) {
    raylib::RenderTexture2D rt = raylib::LoadRenderTexture(32, 32);
    raylib::BeginTextureMode(rt);
    raylib::ClearBackground({0, 0, 0, 255});
    { effects::Effect::Scope scope(e); raylib::DrawRectangle(0, 0, 16, 32, {255, 255, 255, 255}); }
    raylib::EndTextureMode();
    raylib::Image img = raylib::LoadImageFromTexture(rt.texture);
    raylib::ImageFormat(&img, raylib::PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    auto *px = static_cast<raylib::Color *>(img.data);
    raylib::Color c = px[(31 - y) * 32 + x]; // render textures are Y-flipped
    raylib::UnloadImage(img);
    raylib::UnloadRenderTexture(rt);
    return c;
  };

  {
    effects::Effect e = effects::Effect::load_from_memory("uniforms", nullptr, fs);
    check(e.ok(), "fragment-only effect loads from memory");
    e.set("u_vec3", Vector3Type{1.f, 0.f, 0.f});
    e.set("u_vec4", Vector4Type{0.f, 0.5f, 0.f, 0.f});
    e.set("u_float", 0.5f);
    e.set("u_int", 1);
    e.set("u_mat", matrix_compose({0.25f, 0.f, 0.5f}, {0, 0, 0}, {1, 1, 1}));
    raylib::Color c = render_pixel(e, 8, 16);
    // (1,0,0)*0.5 + (0,.5,0) + (0.25,0,0.5) = (0.75, 0.5, 0.5)
    check(std::abs(c.r - 191) <= 2 && std::abs(c.g - 128) <= 2 && std::abs(c.b - 128) <= 2,
          "vec3/vec4/float/int/matrix uniforms reach the fragment shader");
    e.set("u_int", 0);
    raylib::Color off = render_pixel(e, 8, 16);
    check(off.r == 0 && off.g == 0 && off.b == 0, "changing an int uniform changes the pixel");
  }
  {
    effects::Effect e = effects::Effect::load_from_memory("vs_offset", vs, fs);
    check(e.ok(), "effect with a custom vertex shader loads");
    e.set("u_vec3", Vector3Type{0.f, 1.f, 0.f});
    e.set("u_vec4", Vector4Type{0.f, 0.f, 0.f, 0.f});
    e.set("u_float", 1.f);
    e.set("u_int", 1);
    e.set("u_mat", matrix_identity());
    e.set("u_offset", Vector3Type{16.f, 0.f, 0.f});
    raylib::Color moved = render_pixel(e, 24, 16);
    raylib::Color empty = render_pixel(e, 8, 16);
    check(moved.g > 200 && moved.r == 0, "vertex-shader vec3 offset moves the draw");
    check(empty.g == 0, "origin is empty after the vertex offset");
  }

  raylib::CloseWindow();
  printf("effect_uniforms_test: %d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
