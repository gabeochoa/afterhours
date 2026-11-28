#include <cstdio>
#include <filesystem>

#include "../../../../src/drawing_helpers.h"
#include "../../../../src/plugins/color.h"

#define AFTER_HOURS_EXAMPLE_GRAPHICS
#include "../../../example_helper.h"

namespace graphics = afterhours::graphics;
using afterhours::examples::Graphics;

int main() {
  Graphics gfx(Graphics::headless(320, 180));
  if (!gfx.ok()) {
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

  std::printf("rendered offscreen and %s %s\n", saved ? "wrote" : "failed to write",
              png.string().c_str());
  return saved ? 0 : 1;
}
