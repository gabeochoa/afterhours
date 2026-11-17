#include <cstdio>
#include <filesystem>

#include "../../../../src/drawing_helpers.h"
#include "../../../../src/graphics.h"
#include "../../../../src/plugins/color.h"

namespace graphics = afterhours::graphics;

int main() {
  graphics::Config config;
  config.display = graphics::DisplayMode::Headless;
  config.width = 320;
  config.height = 180;
  if (!graphics::init(config)) {
    std::fprintf(stderr, "no offscreen GL context on this machine\n");
    return 1;
  }

  graphics::begin_frame();
  graphics::clear_background(afterhours::Color{20, 24, 32, 255});
  afterhours::draw_rectangle(RectangleType{20, 20, 100, 60},
                             afterhours::Color{255, 120, 0, 255});
  graphics::end_frame();

  const auto png =
      std::filesystem::temp_directory_path() / "offscreen_render.png";
  const bool saved = graphics::capture_frame(png);
  graphics::shutdown();

  std::printf("rendered offscreen and %s %s\n", saved ? "wrote" : "failed to write",
              png.string().c_str());
  return saved ? 0 : 1;
}
