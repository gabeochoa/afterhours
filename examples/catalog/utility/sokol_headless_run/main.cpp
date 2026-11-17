#include <cstdio>
#include <filesystem>

#include "../../../../src/graphics.h"
#include "../../../../src/backends/sokol/drawing_helpers.h"

namespace graphics = afterhours::graphics;

int main() {
  int frames = 0;
  const auto png = std::filesystem::temp_directory_path() / "sokol_headless_run.png";

  graphics::RunConfig config;
  config.display = graphics::DisplayMode::Headless;
  config.width = 320;
  config.height = 180;
  config.frame = [&] {
    graphics::begin_frame();
    graphics::clear_background(afterhours::Color{20, 24, 32, 255});
    afterhours::draw_rectangle(RectangleType{20, 20, 100, 60},
                               afterhours::Color{255, 120, 0, 255});
    graphics::end_frame();
    if (++frames == 10) {
      graphics::capture_frame(png);
      graphics::request_quit();
    }
  };
  graphics::run(config);

  std::printf("ran %d frames headless and wrote %s\n", frames,
              png.string().c_str());
  return frames == 10 ? 0 : 1;
}
