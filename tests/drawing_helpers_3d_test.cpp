#include <afterhours/ah.h>
#include <afterhours/src/drawing_helpers_3d.h>

#include <cmath>
#include <cstdio>
#include <string>

using namespace afterhours;

static int checks_run = 0;
static int checks_passed = 0;

static void check(bool cond, const std::string &what) {
  checks_run++;
  if (cond) {
    checks_passed++;
  } else {
    std::fprintf(stderr, "  FAIL: %s\n", what.c_str());
  }
}

static bool near(float a, float b, float eps = 2.f) {
  return std::fabs(a - b) <= eps;
}

int main() {
  raylib::SetTraceLogLevel(raylib::LOG_ERROR);
  raylib::SetConfigFlags(raylib::FLAG_WINDOW_HIDDEN);
  raylib::InitWindow(64, 64, "drawing_helpers_3d_test");

  Camera3D cam{};
  cam.position = {0.f, 0.f, 5.f};
  cam.target = {0.f, 0.f, 0.f};
  cam.up = {0.f, 1.f, 0.f};
  cam.fovy = 45.f;
  cam.projection = raylib::CAMERA_PERSPECTIVE;

  Vector2Type wide = get_world_to_screen(Vector3Type{0.f, 0.f, 0.f}, cam, 800, 600);
  check(near(wide.x, 400.f) && near(wide.y, 300.f),
        "origin projects to centre of an 800x600 target");

  Vector2Type small = get_world_to_screen(Vector3Type{0.f, 0.f, 0.f}, cam, 400, 200);
  check(near(small.x, 200.f) && near(small.y, 100.f),
        "origin projects to centre of a 400x200 target");

  Vector2Type windowed = get_world_to_screen(Vector3Type{0.f, 0.f, 0.f}, cam);
  check(near(windowed.x, 32.f) && near(windowed.y, 32.f),
        "2-arg overload still defaults to the 64x64 window size");

  raylib::RenderTexture2D rt = raylib::LoadRenderTexture(64, 64);
  check(rt.id != 0, "render texture loads");
  raylib::BeginTextureMode(rt);
  raylib::ClearBackground(raylib::Color{0, 0, 0, 255});
  begin_3d(cam);
  draw_cylinder_ex(Vector3Type{0.f, -2.f, 0.f}, Vector3Type{0.f, 2.f, 0.f},
                   1.f, 1.f, 16, Color{230, 41, 55, 255});
  end_3d();
  raylib::EndTextureMode();

  raylib::Image img = raylib::LoadImageFromTexture(rt.texture);
  raylib::ImageFormat(&img, raylib::PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
  auto *pixels = static_cast<unsigned char *>(img.data);
  int covered = 0;
  for (int i = 0; i < 64 * 64; i++) {
    if (pixels[i * 4] > 100) covered++;
  }
  check(covered > 100, "draw_cylinder_ex renders non-background pixels");
  raylib::UnloadImage(img);
  raylib::UnloadRenderTexture(rt);

  raylib::CloseWindow();
  std::printf("%d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
