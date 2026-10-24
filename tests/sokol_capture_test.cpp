#include <afterhours/src/graphics.h>
#include <afterhours/src/backends/sokol/drawing_helpers.h>
#include <afterhours/src/backends/sokol/font_helper.h>

#include "ui_test_harness.h"

#include <bitset>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace g = afterhours::graphics;
using afterhours::Color;
// The developer.h fallback types live in the global namespace.
using ::RectangleType;
using ::TextureType;
using ::Vector2Type;

static int checks_run = 0;
static int checks_passed = 0;

static void check(bool cond, const char *what) {
  checks_run++;
  if (cond) {
    checks_passed++;
  } else {
    fprintf(stderr, "  FAIL: %s\n", what);
  }
}

static bool has_op(const char *op) {
  for (const auto &c : afterhours::capture::calls())
    if (c.op == op)
      return true;
  return false;
}

int main() {
  printf("=== sokol capture tests ===\n\n");

  g::Config cfg;
  cfg.display = g::DisplayMode::Headless;
  cfg.width = 800;
  cfg.height = 600;
  cfg.title = "capture test";
  if (!g::init(cfg)) {
    // No GPU (CI container, remote session). Skip rather than fail: a red
    // suite for "this machine has no Metal device" teaches nothing.
    printf("SKIP: headless Metal init failed (no GPU available)\n");
    return 0;
  }

  afterhours::capture::enable();

  // Direct draws, one of each primitive the backend records.
  {
    afterhours::capture::clear();
    g::begin_drawing();
    afterhours::draw_rectangle(RectangleType{10, 10, 50, 40},
                               Color{255, 0, 0, 255});
    afterhours::draw_rectangle_outline(RectangleType{10, 10, 50, 40},
                                       Color{0, 255, 0, 255});
    afterhours::draw_rectangle_rounded(RectangleType{10, 10, 50, 40}, 0.5f, 8,
                                       Color{0, 0, 255, 255},
                                       std::bitset<4>().set());
    afterhours::draw_rectangle_rounded_lines(RectangleType{10, 10, 50, 40}, 0.5f, 8,
                                             Color{255, 255, 0, 255},
                                             std::bitset<4>().set());
    afterhours::draw_line_ex(Vector2Type{0, 0}, Vector2Type{100, 100}, 2.0f,
                             Color{0, 255, 255, 255});
    afterhours::draw_circle_v(Vector2Type{50, 50}, 10, Color{255, 0, 255, 255});
    afterhours::draw_triangle(Vector2Type{0, 0}, Vector2Type{10, 0},
                              Vector2Type{0, 10}, Color{128, 128, 128, 255});
    const Vector2Type quad[4] = {{0, 0}, {10, 0}, {10, 10}, {0, 10}};
    afterhours::draw_quad(quad, Color{64, 64, 64, 255});
    TextureType tex{};
    afterhours::draw_texture_quad(tex, quad, Color{255, 255, 255, 255});
    g::end_drawing();
    check(has_op("rectangle"), "rectangle recorded");
    check(has_op("rectangle_outline"), "rectangle_outline recorded");
    check(has_op("rectangle_rounded"), "rectangle_rounded recorded");
    check(has_op("rectangle_rounded_lines"), "rectangle_rounded_lines recorded");
    check(has_op("line"), "line recorded");
    check(has_op("circle"), "circle recorded");
    check(has_op("triangle"), "triangle recorded");
    check(has_op("quad"), "quad recorded");
    check(has_op("texture_quad"), "texture_quad recorded");
  }

  // Text goes through the font path: load a face so the backend has a font
  // context, then draw through both text entry points.
  {
    const auto path = std::filesystem::path(AFTERHOURS_TEST_FIXTURES) /
                      "AtkinsonHyperlegible-Regular.ttf";
    const afterhours::Font font = afterhours::load_font_from_file(path.c_str());
    check(font.id != FONS_INVALID, "fixture font loads");
    afterhours::capture::clear();
    g::begin_drawing();
    afterhours::draw_text_ex(font, "hello", Vector2Type{10, 10}, 16, 0,
                             Color{255, 255, 255, 255});
    afterhours::draw_text("world", 10, 30, 16, Color{255, 255, 255, 255});
    g::end_drawing();
    bool saw_hello = false, saw_world = false;
    for (const auto &c : afterhours::capture::calls()) {
      if (c.op == "text" && c.text == "hello")
        saw_hello = true;
      if (c.op == "text" && c.text == "world")
        saw_world = true;
    }
    check(saw_hello, "text records draw_text_ex content");
    check(saw_world, "text records draw_text content");
  }

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
