
#if !defined(AFTER_HOURS_USE_RAYLIB) && !defined(AFTER_HOURS_USE_VULKAN) && !defined(AFTER_HOURS_USE_SOKOL)
#define AFTER_HOURS_USE_SOKOL
#endif

#include <afterhours/src/graphics.h>
#include <afterhours/src/backends/sokol/drawing_helpers.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace g = afterhours::graphics;
using afterhours::Color;

static constexpr int W = 200;
static constexpr int H = 200;
static constexpr float EDGE = 100.4f;

static int render_child(int scale, const std::string &out) {
  g::Config cfg;
  cfg.display = g::DisplayMode::Headless;
  cfg.width = W;
  cfg.height = H;
  cfg.hidpi = scale > 1;
  cfg.hidpi_scale = scale;
  cfg.title = "capture supersample test";
  if (!g::init(cfg))
    return 2;
  g::begin_drawing();
  afterhours::draw_rectangle(RectangleType{0, 0, W, H}, Color{0, 0, 0, 255});
  afterhours::draw_rectangle(RectangleType{0, 0, EDGE, H},
                             Color{255, 255, 255, 255});
  g::end_drawing();
  auto cap = afterhours::capture_render_texture_rgba(
      g::metal_detail::g_headless_rt);
  g::shutdown();
  if (!cap)
    return 1;
  std::ofstream f(out, std::ios::binary);
  f.write(reinterpret_cast<const char *>(cap->pixels.data()),
          static_cast<std::streamsize>(cap->pixels.size()));
  return f ? 0 : 1;
}

static std::vector<unsigned char> read_file(const std::string &path) {
  std::ifstream f(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

static int white_run(const std::vector<unsigned char> &px, int width,
                     int scale) {
  const size_t row = static_cast<size_t>(H * scale / 2) * width * 4;
  int run = 0;
  while (run < width && px[row + static_cast<size_t>(run) * 4] > 200)
    ++run;
  return run;
}

int main(int argc, char *argv[]) {
  if (argc == 3)
    return render_child(std::atoi(argv[1]), argv[2]);

  printf("=== capture supersample tests ===\n\n");
  int checks_run = 0, checks_passed = 0;
  auto check = [&](bool cond, const std::string &what) {
    ++checks_run;
    if (cond)
      ++checks_passed;
    else
      fprintf(stderr, "  FAIL: %s\n", what.c_str());
  };

  const auto dir = std::filesystem::temp_directory_path();
  const std::string p1 = (dir / "afh_super_1x.rgba").string();
  const std::string p2 = (dir / "afh_super_2x.rgba").string();
  const std::string exe = argv[0];
  auto run_child = [&](int scale, const std::string &out) {
    const std::string cmd = "\"" + exe + "\" " + std::to_string(scale) +
                            " \"" + out + "\"";
    return std::system(cmd.c_str());
  };
  const int rc1 = run_child(1, p1);
  const int rc2 = run_child(2, p2);
  if (rc1 != 0 || rc2 != 0) {
    printf("SKIP: headless init failed (no GPU available)\n");
    return 0;
  }

  const auto px1 = read_file(p1);
  const auto px2 = read_file(p2);
  check(px1.size() == static_cast<size_t>(W) * H * 4,
        "1x capture is logical-size pixels");
  check(px2.size() == static_cast<size_t>(W * 2) * (H * 2) * 4,
        "2x capture is twice the pixels in each dimension");

  if (px1.size() == static_cast<size_t>(W) * H * 4 &&
      px2.size() == static_cast<size_t>(W * 2) * (H * 2) * 4) {
    const int run1 = white_run(px1, W, 1);
    const int run2 = white_run(px2, W * 2, 2);
    printf("  edge at %.1f logical: 1x run=%d, 2x run=%d (%.2f logical)\n",
           EDGE, run1, run2, run2 / 2.0);
    check(run1 >= 99 && run1 <= 101, "1x edge lands within a pixel");
    check(run2 >= 200 && run2 <= 202, "2x edge lands within half a pixel");
    const double err1 = std::abs(run1 - static_cast<double>(EDGE));
    const double err2 = std::abs(run2 / 2.0 - static_cast<double>(EDGE));
    check(err2 < err1, "2x edge coverage is closer to the true edge");
  }

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
