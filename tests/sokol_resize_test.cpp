#define AFTER_HOURS_USE_METAL

#include <afterhours/src/graphics.h>
#include <afterhours/src/backends/sokol/drawing_helpers.h>

#include <climits>
#include <cstdio>
#include <cstdlib>

namespace g = afterhours::graphics;
using afterhours::Color;

static int checks = 0;
static int failures = 0;

static void check(bool condition, const char *message) {
  ++checks;
  if (condition) return;
  ++failures;
  std::fprintf(stderr, "FAIL: %s\n", message);
}

static void check_resources(const g::RenderTextureType &rt, bool alive) {
  const auto valid = [alive](sg_resource_state state) {
    return (state == SG_RESOURCESTATE_VALID) == alive;
  };
  check(valid(sg_query_image_state({rt.color_img_id})), "color image lifetime");
  check(valid(sg_query_image_state({rt.depth_img_id})), "depth image lifetime");
  check(valid(sg_query_view_state({rt.color_view_id})), "color view lifetime");
  check(valid(sg_query_view_state({rt.depth_view_id})), "depth view lifetime");
  check(valid(sg_query_view_state({rt.tex_view_id})), "sampled view lifetime");
  check(valid(sg_query_sampler_state({rt.sampler_id})), "sampler lifetime");
}

static void paint() {
  const float width = static_cast<float>(g::get_screen_width());
  const float height = static_cast<float>(g::get_screen_height());
  afterhours::draw_rectangle(RectangleType{0, 0, width, height}, Color{0, 0, 255, 255});
  afterhours::draw_rectangle(RectangleType{0, 0, width / 2, height}, Color{0, 255, 0, 255});
}

static void check_capture(int width, int height, int scale) {
  const auto &rt = g::get_render_texture();
  check(g::get_screen_width() == width && g::get_screen_height() == height,
        "logical screen dimensions reflect applied resize");
  check(rt.width == width * scale && rt.height == height * scale,
        "physical target dimensions retain DPI scale");
  check(rt.scale == scale && g::render_scale() == scale, "render scale survives resize");
  const auto capture = afterhours::capture_render_texture_rgba(rt);
  check(capture.has_value(), "capture succeeds after resize");
  if (!capture) return;
  const auto &pixels = capture->pixels;
  check(pixels.size() == static_cast<size_t>(width * scale) * height * scale * 4,
        "capture contains full physical framebuffer");
  if (pixels.size() != static_cast<size_t>(width * scale) * height * scale * 4) return;
  const size_t left = (static_cast<size_t>(height * scale / 2) * width * scale + width * scale / 4) * 4;
  const size_t right = (static_cast<size_t>(height * scale / 2) * width * scale + width * scale * 3 / 4) * 4;
  check(pixels[left] < 6 && pixels[left + 1] > 249 && pixels[left + 2] < 6,
        "logical left half fills left half of physical target");
  check(pixels[right] < 6 && pixels[right + 1] < 6 && pixels[right + 2] > 249,
        "logical right half fills right half of physical target");
}

int main(int argc, char **argv) {
  const int scale = argc > 1 ? std::atoi(argv[1]) : 2;
  g::Config config;
  config.display = g::DisplayMode::Headless;
  config.width = 80;
  config.height = 60;
  config.hidpi = scale > 1;
  config.hidpi_scale = scale;
  if (!g::init(config)) {
    std::fprintf(stderr, "FAIL: Metal headless initialization failed\n");
    return 1;
  }
  const auto initial = g::get_render_texture();
  g::begin_frame();
  paint();
  g::set_window_size(120, 90);
  check(g::get_render_texture().color_img_id == initial.color_img_id,
        "request inside active pass preserves target");
  check(g::get_screen_width() == 80, "mid-frame request preserves applied logical size");
  check_resources(initial, true);
  paint();
  g::end_frame();
  check_capture(80, 60, scale);
  g::begin_frame();
  check_resources(initial, false);
  check_resources(g::get_render_texture(), true);
  check(g::get_render_texture().sgl_ctx_id == initial.sgl_ctx_id,
        "resize reuses shared drawing context");
  paint();
  g::end_frame();
  check_capture(120, 90, scale);

  const auto before_coalescing = g::get_render_texture();
  g::set_window_size(130, 95);
  g::set_window_size(160, 100);
  g::set_window_size(0, 120);
  g::set_window_size(-1, 120);
  g::set_window_size(INT_MAX, INT_MAX);
  check(g::get_render_texture().color_img_id == before_coalescing.color_img_id,
        "between-frame requests wait for next frame");
  g::begin_drawing();
  paint();
  g::end_drawing();
  check_capture(160, 100, scale);
  check_resources(before_coalescing, false);

  const auto before_cancel = g::get_render_texture();
  g::set_window_size(200, 110);
  g::set_window_size(160, 100);
  g::begin_frame();
  paint();
  g::end_frame();
  check(g::get_render_texture().color_img_id == before_cancel.color_img_id,
        "latest request for current size cancels pending allocation");

  auto child = afterhours::load_render_texture(24, 24);
  g::begin_frame();
  afterhours::begin_texture_mode(child);
  g::set_window_size(96, 72);
  afterhours::draw_rectangle(RectangleType{0, 0, 24, 24}, Color{255, 0, 0, 255});
  check_resources(before_cancel, true);
  afterhours::end_texture_mode();
  g::end_frame();
  g::begin_frame();
  paint();
  g::end_frame();
  check_capture(96, 72, scale);
  check_resources(before_cancel, false);
  check_resources(child, true);
  afterhours::unload_render_texture(child);

  for (int index = 0; index < 64; ++index) {
    const auto prior = g::get_render_texture();
    const int width = 80 + index % 3 * 8;
    const int height = 60 + index % 5 * 4;
    g::begin_frame();
    paint();
    g::set_window_size(width, height);
    g::end_frame();
    g::begin_frame();
    paint();
    g::end_frame();
    check_resources(g::get_render_texture(), true);
    if (prior.color_img_id != g::get_render_texture().color_img_id)
      check_resources(prior, false);
    check(g::get_render_texture().sgl_ctx_id == initial.sgl_ctx_id,
          "repeated resizes reuse drawing context");
    check_capture(width, height, scale);
  }
  g::set_window_size(100, 100);
  g::shutdown();
  check(!g::metal_detail::g_pending_headless_size.has_value(), "shutdown clears pending resize");
  std::printf("%d/%d checks passed (scale %d)\n", checks - failures, checks, scale);
  return failures == 0 ? 0 : 1;
}
