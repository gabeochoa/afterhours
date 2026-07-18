#include <afterhours/src/graphics.h>

// Compile-only smoke helper: validates the new graphics API surface compiles
// under both backend macro configurations.
int backend_graphics_smoke() {
  using namespace afterhours::graphics;
  RenderTextureType rt{};
  set_window_state(FLAG_VSYNC_HINT);
  clear_window_state(FLAG_VSYNC_HINT);
  set_window_size(640, 360);
  set_window_min_size(320, 180);
  set_render_texture_filter(rt, TEXTURE_FILTER_POINT);
  return 0;
}
