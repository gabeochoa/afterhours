// presented_frame_test.cpp
// Canvas capture reads the render texture; the player sees that texture
// letterboxed into the window. This checks the presented capture is
// window-sized, with the scene inside the letterbox rect and bars outside.

#include <afterhours/src/backends/raylib/backend.h>
#include <afterhours/src/backends/raylib/headless.h>
#include <afterhours/src/graphics_common.h>

#include <cstdio>
#include <filesystem>
#include <string>

using namespace afterhours;

static int checks = 0;
static int failures = 0;
static void check(bool cond, const std::string &what) {
  checks++;
  if (!cond) {
    failures++;
    std::printf("  FAIL: %s\n", what.c_str());
  }
}
static bool near(float a, float b) { return std::abs(a - b) < 0.6f; }

int main() {
  // Pure geometry: 128x72 canvas into a 200x100 window is height-limited.
  const auto pf = graphics::presented_frame(128, 72, 200, 100);
  check(pf.window_w == 200 && pf.window_h == 100, "geometry keeps window size");
  check(near(pf.dest_w, 178.f) && near(pf.dest_h, 100.f),
        "geometry letterboxes to 178x100");
  check(near(pf.dest_x, 11.f) && near(pf.dest_y, 0.f), "geometry centres bars");
  check(pf.scale > 0.71f && pf.scale < 0.73f, "geometry scale canvas/window");

  graphics::Config cfg;
  cfg.display = graphics::DisplayMode::Headless;
  cfg.width = 128;
  cfg.height = 72;
  graphics::raylib_backend::ensure_registered();
  check(graphics::init(cfg), "headless backend init");

  graphics::begin_frame();
  raylib::ClearBackground(raylib::Color{255, 0, 0, 255});
  raylib::DrawRectangle(0, 0, 64, 72, raylib::Color{0, 0, 255, 255});
  graphics::end_frame();

  const auto path =
      std::filesystem::temp_directory_path() / "afh_presented_frame_test.png";
  check(graphics::capture_presented_frame(path, 200, 100),
        "presented capture succeeds");
  // Default canvas capture stays canvas-sized (unchanged behaviour).
  const auto canvas_path =
      std::filesystem::temp_directory_path() / "afh_canvas_frame_test.png";
  check(graphics::capture_frame(canvas_path), "canvas capture succeeds");

  raylib::Image presented = raylib::LoadImage(path.string().c_str());
  raylib::Image canvas = raylib::LoadImage(canvas_path.string().c_str());
  check(presented.data != nullptr && presented.width == 200 &&
            presented.height == 100,
        "presented PNG is window size, not canvas size");
  check(canvas.data != nullptr && canvas.width == 128 && canvas.height == 72,
        "canvas PNG stays canvas size");
  if (presented.data) {
    const auto bar = raylib::GetImageColor(presented, 2, 50);
    check(bar.r == 0 && bar.g == 0 && bar.b == 0, "left bar is black");
    // Dest starts at x=11; its left half is the canvas's blue left half.
    const auto inside = raylib::GetImageColor(presented, 20, 50);
    check(inside.b == 255 && inside.r == 0, "known pixel inside letterbox");
    const auto right = raylib::GetImageColor(presented, 150, 50);
    check(right.r == 255 && right.b == 0, "red half inside letterbox");
  }
  if (presented.data) raylib::UnloadImage(presented);
  if (canvas.data) raylib::UnloadImage(canvas);
  graphics::shutdown();
  std::filesystem::remove(path);
  std::filesystem::remove(canvas_path);

  std::printf("%d/%d checks passed\n", checks - failures, checks);
  return failures == 0 ? 0 : 1;
}
