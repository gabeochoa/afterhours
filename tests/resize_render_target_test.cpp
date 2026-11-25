// resize_render_target_test.cpp
// The render texture is allocated once at init; resizing must reallocate it
// at the new logical size * scale and keep render_scale() in sync.

#include <afterhours/src/graphics.h>

#include <cstdio>
#include <filesystem>
#include <string>

using namespace afterhours;

static int checks_run = 0;
static int checks_passed = 0;

static void check(bool cond, const std::string &what) {
  checks_run++;
  if (cond) {
    checks_passed++;
  } else {
    fprintf(stderr, "  FAIL: %s\n", what.c_str());
  }
}

int main() {
  graphics::Config cfg;
  cfg.display = graphics::DisplayMode::Headless;
  cfg.width = 320;
  cfg.height = 200;
  graphics::raylib_backend::ensure_registered();
  check(graphics::init(cfg), "headless init at 320x200 scale 1");
  const auto &initial = graphics::get_render_texture();
  check(initial.texture.width == 320 && initial.texture.height == 200,
        "initial texture is 320x200");
  check(graphics::render_scale() == 1, "initial render_scale is 1");

  check(graphics::resize_render_target(400, 300, 2), "resize to 400x300@2");
  const auto &resized = graphics::get_render_texture();
  check(resized.texture.width == 800 && resized.texture.height == 600,
        "resized texture is 800x600");
  check(graphics::render_scale() == 2, "render_scale is 2 after resize");

  graphics::begin_frame();
  raylib::ClearBackground(raylib::Color{0, 0, 255, 255});
  raylib::DrawRectangle(0, 0, 200, 150, raylib::Color{0, 255, 0, 255});
  graphics::end_frame();
  const auto path = std::filesystem::temp_directory_path() /
                    "afh_resize_render_target_test.png";
  check(graphics::capture_frame(path), "capture succeeds at new dimensions");
  raylib::Image captured = raylib::LoadImage(path.string().c_str());
  check(captured.data != nullptr && captured.width == 800 &&
            captured.height == 600,
        "captured PNG is 800x600");
  if (captured.data)
    raylib::UnloadImage(captured);
  std::filesystem::remove(path);

  const unsigned int texture_id = graphics::get_render_texture().texture.id;
  const unsigned int framebuffer_id = graphics::get_render_texture().id;
  check(graphics::resize_render_target(400, 300, 2), "same-size resize");
  check(graphics::get_render_texture().texture.id == texture_id &&
            graphics::get_render_texture().id == framebuffer_id,
        "same-size resize is a no-op keeping the texture");

  check(!graphics::resize_render_target(0, 300, 2), "width 0 rejected");
  check(!graphics::resize_render_target(400, -1, 2), "negative height rejected");
  check(graphics::get_render_texture().texture.id == texture_id &&
            graphics::get_render_texture().texture.width == 800,
        "invalid resize leaves the texture intact");

  check(graphics::resize_render_target(320, 200), "resize with default scale");
  check(graphics::get_render_texture().texture.width == 640 &&
            graphics::get_render_texture().texture.height == 400 &&
            graphics::render_scale() == 2,
        "default scale keeps the current scale");

  graphics::shutdown();
  printf("resize_render_target_test: %d/%d checks passed\n", checks_passed,
         checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
