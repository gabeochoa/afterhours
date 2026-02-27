// headless_validation.cpp
// Validates pixel-identical output between headless and windowed rendering
// backends. Renders the same scenes through both backends and compares the
// resulting PNGs pixel-by-pixel. Failures produce diff images showing
// mismatched pixels.
//
// Build:
//   cd examples && make headless_validation
//
// Run:
//   ./headless_validation                        # run all scenes
//   ./headless_validation --scene=filled_rects   # run one scene
//   ./headless_validation --output-dir=out       # custom output directory

#define FMT_HEADER_ONLY
#include <fmt/format.h>

#include <afterhours/ah.h>
#include <afterhours/src/drawing_helpers.h>
#include <afterhours/src/graphics.h>
#include <afterhours/src/plugins/autolayout.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace graphics = afterhours::graphics;
using namespace afterhours;
using namespace afterhours::ui;

static constexpr int RENDER_W = 800;
static constexpr int RENDER_H = 600;

// ============================================================================
// Layout test harness (mirrors autolayout_test.cpp's TestLayout)
// ============================================================================

struct TestLayout {
  std::vector<std::unique_ptr<Entity>> entities;
  window_manager::Resolution resolution{RENDER_W, RENDER_H};
  float ui_scale = 1.0f;

  Entity &make_entity() {
    entities.push_back(std::make_unique<Entity>());
    return *entities.back();
  }

  Entity &make_ui(Size w, Size h) {
    Entity &e = make_entity();
    auto &ui = e.addComponent<UIComponent>(e.id);
    ui.set_desired_width(w);
    ui.set_desired_height(h);
    return e;
  }

  void add_child(Entity &parent, Entity &child) {
    auto &pc = parent.get<UIComponent>();
    auto &cc = child.get<UIComponent>();
    pc.add_child(child.id);
    cc.set_parent(parent.id);
  }

  void run(Entity &root) {
    EntityID max_id = 0;
    for (auto &e : entities) {
      max_id = std::max(max_id, e->id);
    }
    std::vector<Entity *> mapping(static_cast<size_t>(max_id) + 1, nullptr);
    for (auto &e : entities) {
      mapping[e->id] = e.get();
    }
    AutoLayout::autolayout(root.get<UIComponent>(), resolution, mapping,
                           /*enable_grid_snapping=*/false, ui_scale);
  }

  static UIComponent &ui(Entity &e) { return e.get<UIComponent>(); }
};

// Helper: draw a UIComponent tree as colored rectangles for visual comparison.
// Each depth level gets a different color for clarity.
static void draw_layout_tree(TestLayout &t, Entity &e, int depth = 0) {
  static const raylib::Color palette[] = {
      {230, 80, 80, 255},   // red
      {80, 180, 80, 255},   // green
      {80, 100, 230, 255},  // blue
      {230, 180, 50, 255},  // yellow
      {180, 80, 220, 255},  // purple
      {80, 200, 200, 255},  // cyan
      {230, 130, 60, 255},  // orange
      {160, 160, 160, 255}, // gray
  };
  constexpr int palette_size = sizeof(palette) / sizeof(palette[0]);

  auto &ui = t.ui(e);
  if (ui.should_hide)
    return;

  auto rect = ui.rect();
  raylib::Color c = palette[depth % palette_size];
  draw_rectangle(rect, c);

  // Outline for visibility
  draw_rectangle_outline(rect, raylib::Color{0, 0, 0, 255}, 1.0f);

  for (auto child_id : ui.children) {
    for (auto &ent : t.entities) {
      if (ent->id == child_id) {
        draw_layout_tree(t, *ent, depth + 1);
        break;
      }
    }
  }
}

// ============================================================================
// Scene definitions
// ============================================================================

struct Scene {
  const char *name;
  std::function<void()> render;
};

// -- Tier 1: Primitives ------------------------------------------------------

static void scene_filled_rects() {
  raylib::ClearBackground(raylib::Color{40, 40, 40, 255});

  draw_rectangle(raylib::Rectangle{50, 50, 200, 100},
                 raylib::Color{230, 80, 80, 255});
  draw_rectangle(raylib::Rectangle{300, 50, 150, 150},
                 raylib::Color{80, 180, 80, 255});
  draw_rectangle(raylib::Rectangle{500, 100, 250, 80},
                 raylib::Color{80, 100, 230, 255});
  // Overlapping
  draw_rectangle(raylib::Rectangle{100, 200, 300, 200},
                 raylib::Color{230, 180, 50, 200});
  draw_rectangle(raylib::Rectangle{250, 300, 300, 200},
                 raylib::Color{180, 80, 220, 200});
}

static void scene_outlined_rects() {
  raylib::ClearBackground(raylib::Color{240, 240, 240, 255});

  draw_rectangle_outline(raylib::Rectangle{50, 50, 200, 100},
                         raylib::Color{230, 80, 80, 255}, 1.0f);
  draw_rectangle_outline(raylib::Rectangle{300, 50, 150, 150},
                         raylib::Color{80, 180, 80, 255}, 2.0f);
  draw_rectangle_outline(raylib::Rectangle{500, 50, 250, 200},
                         raylib::Color{80, 100, 230, 255}, 4.0f);
  draw_rectangle_outline(raylib::Rectangle{50, 300, 700, 250},
                         raylib::Color{0, 0, 0, 255}, 3.0f);
}

static void scene_rounded_rects() {
  raylib::ClearBackground(raylib::Color{245, 245, 245, 255});

  // All corners rounded
  draw_rectangle_rounded(raylib::Rectangle{50, 50, 200, 120}, 0.3f, 8,
                         raylib::Color{100, 150, 230, 255},
                         std::bitset<4>("1111"));
  // Two corners
  draw_rectangle_rounded(raylib::Rectangle{300, 50, 200, 120}, 0.5f, 8,
                         raylib::Color{230, 100, 100, 255},
                         std::bitset<4>("1100"));
  // One corner
  draw_rectangle_rounded(raylib::Rectangle{550, 50, 200, 120}, 0.8f, 12,
                         raylib::Color{100, 200, 100, 255},
                         std::bitset<4>("1000"));
  // No rounding (falls through to regular rectangle)
  draw_rectangle_rounded(raylib::Rectangle{50, 250, 200, 120}, 0.3f, 8,
                         raylib::Color{200, 200, 80, 255},
                         std::bitset<4>("0000"));
  // Small radius
  draw_rectangle_rounded(raylib::Rectangle{300, 250, 200, 120}, 0.1f, 4,
                         raylib::Color{200, 100, 200, 255},
                         std::bitset<4>("1111"));
}

static void scene_circles_lines_triangles() {
  raylib::ClearBackground(raylib::Color{30, 30, 50, 255});

  // Circles
  draw_circle(100, 100, 60.0f, raylib::Color{200, 60, 60, 255});
  draw_circle(250, 100, 40.0f, raylib::Color{60, 200, 60, 255});
  draw_circle_lines(400, 100, 50.0f, raylib::Color{60, 60, 200, 255});

  // Lines
  draw_line(50, 250, 750, 250, raylib::Color{255, 255, 255, 255});
  draw_line_ex(raylib::Vector2{50, 300}, raylib::Vector2{750, 350}, 3.0f,
               raylib::Color{255, 200, 50, 255});

  // Triangles
  draw_triangle(raylib::Vector2{150, 500}, raylib::Vector2{50, 400},
                raylib::Vector2{250, 400}, raylib::Color{255, 100, 200, 255});
  draw_triangle_lines(raylib::Vector2{450, 500}, raylib::Vector2{350, 400},
                      raylib::Vector2{550, 400},
                      raylib::Color{100, 255, 200, 255});

  // Ellipse
  draw_ellipse(650, 450, 80.0f, 50.0f, raylib::Color{200, 200, 60, 255});
}

static void scene_text() {
  raylib::ClearBackground(raylib::Color{255, 255, 255, 255});

  draw_text("Hello, World!", 50, 50, 30, raylib::Color{0, 0, 0, 255});
  draw_text("Small text", 50, 120, 14, raylib::Color{100, 100, 100, 255});
  draw_text("LARGE TEXT", 50, 170, 48, raylib::Color{200, 50, 50, 255});
  draw_text("Overlapping", 300, 300, 36, raylib::Color{0, 0, 200, 180});
  draw_text("Text layers", 310, 310, 36, raylib::Color{200, 0, 0, 180});
}

static void scene_alpha_blending() {
  raylib::ClearBackground(raylib::Color{255, 255, 255, 255});

  // Draw a checkerboard base pattern
  for (int y = 0; y < RENDER_H; y += 40) {
    for (int x = 0; x < RENDER_W; x += 40) {
      unsigned char v = ((x / 40 + y / 40) % 2 == 0) ? 200 : 230;
      draw_rectangle(raylib::Rectangle{(float)x, (float)y, 40, 40},
                     raylib::Color{v, v, v, 255});
    }
  }

  // Semi-transparent overlays at various alpha levels
  draw_rectangle(raylib::Rectangle{50, 50, 200, 200},
                 raylib::Color{255, 0, 0, 64});
  draw_rectangle(raylib::Rectangle{150, 50, 200, 200},
                 raylib::Color{0, 255, 0, 128});
  draw_rectangle(raylib::Rectangle{250, 50, 200, 200},
                 raylib::Color{0, 0, 255, 192});

  // Overlapping semi-transparent circles
  draw_circle(400, 400, 100.0f, raylib::Color{255, 0, 0, 100});
  draw_circle(470, 400, 100.0f, raylib::Color{0, 255, 0, 100});
  draw_circle(435, 340, 100.0f, raylib::Color{0, 0, 255, 100});
}

// -- Tier 2: Layout ----------------------------------------------------------

static void scene_flex_column() {
  raylib::ClearBackground(raylib::Color{50, 50, 50, 255});
  TestLayout t;

  auto &root = t.make_ui(pixels(RENDER_W), pixels(RENDER_H));
  t.ui(root).set_flex_direction(FlexDirection::Column);
  t.ui(root).set_desired_padding(pixels(20), Axis::X);
  t.ui(root).set_desired_padding(pixels(20), Axis::Y);

  auto &a = t.make_ui(percent(1.0f), pixels(100));
  auto &b = t.make_ui(percent(1.0f), pixels(150));
  auto &c = t.make_ui(percent(1.0f), pixels(80));
  t.add_child(root, a);
  t.add_child(root, b);
  t.add_child(root, c);

  t.run(root);
  draw_layout_tree(t, root);
}

static void scene_flex_row() {
  raylib::ClearBackground(raylib::Color{50, 50, 50, 255});
  TestLayout t;

  auto &root = t.make_ui(pixels(RENDER_W), pixels(RENDER_H));
  t.ui(root).set_flex_direction(FlexDirection::Row);
  t.ui(root).set_desired_padding(pixels(20), Axis::X);
  t.ui(root).set_desired_padding(pixels(20), Axis::Y);

  auto &a = t.make_ui(pixels(200), percent(1.0f));
  auto &b = t.make_ui(pixels(300), percent(1.0f));
  auto &c = t.make_ui(pixels(150), percent(1.0f));
  t.add_child(root, a);
  t.add_child(root, b);
  t.add_child(root, c);

  t.run(root);
  draw_layout_tree(t, root);
}

static void scene_flex_grow_weighted() {
  raylib::ClearBackground(raylib::Color{50, 50, 50, 255});
  TestLayout t;

  auto &root = t.make_ui(pixels(RENDER_W), pixels(RENDER_H));
  t.ui(root).set_flex_direction(FlexDirection::Row);
  t.ui(root).set_desired_padding(pixels(10), Axis::X);
  t.ui(root).set_desired_padding(pixels(10), Axis::Y);

  // 1:2:1 weight ratio
  auto &a = t.make_ui(expand(1), percent(1.0f));
  auto &b = t.make_ui(expand(2), percent(1.0f));
  auto &c = t.make_ui(expand(1), percent(1.0f));
  t.add_child(root, a);
  t.add_child(root, b);
  t.add_child(root, c);

  t.run(root);
  draw_layout_tree(t, root);
}

static void scene_justify_content() {
  raylib::ClearBackground(raylib::Color{45, 45, 55, 255});
  TestLayout t;

  auto &root = t.make_ui(pixels(RENDER_W), pixels(RENDER_H));
  t.ui(root).set_flex_direction(FlexDirection::Column);

  // Five rows, each demonstrating a different JustifyContent
  JustifyContent modes[] = {
      JustifyContent::FlexStart,   JustifyContent::FlexEnd,
      JustifyContent::Center,      JustifyContent::SpaceBetween,
      JustifyContent::SpaceAround,
  };

  for (int i = 0; i < 5; i++) {
    auto &row = t.make_ui(percent(1.0f), pixels(100));
    t.ui(row).set_flex_direction(FlexDirection::Row);
    t.ui(row).justify_content = modes[i];
    t.ui(row).set_desired_padding(pixels(5), Axis::X);
    t.ui(row).set_desired_padding(pixels(5), Axis::Y);
    t.add_child(root, row);

    for (int j = 0; j < 3; j++) {
      auto &item = t.make_ui(pixels(80), pixels(60));
      t.add_child(row, item);
    }
  }

  t.run(root);
  draw_layout_tree(t, root);
}

static void scene_align_items() {
  raylib::ClearBackground(raylib::Color{55, 45, 45, 255});
  TestLayout t;

  auto &root = t.make_ui(pixels(RENDER_W), pixels(RENDER_H));
  t.ui(root).set_flex_direction(FlexDirection::Column);

  AlignItems modes[] = {
      AlignItems::FlexStart,
      AlignItems::FlexEnd,
      AlignItems::Center,
      AlignItems::Stretch,
  };

  for (int i = 0; i < 4; i++) {
    auto &row = t.make_ui(percent(1.0f), pixels(130));
    t.ui(row).set_flex_direction(FlexDirection::Row);
    t.ui(row).align_items = modes[i];
    t.ui(row).set_desired_padding(pixels(5), Axis::X);
    t.ui(row).set_desired_padding(pixels(5), Axis::Y);
    t.add_child(root, row);

    // Different height children to show alignment effect
    auto &a = t.make_ui(pixels(100), pixels(40));
    auto &b = t.make_ui(pixels(100), pixels(80));
    auto &c = t.make_ui(pixels(100), pixels(60));
    t.add_child(row, a);
    t.add_child(row, b);
    t.add_child(row, c);
  }

  t.run(root);
  draw_layout_tree(t, root);
}

static void scene_wrap_with_gap() {
  raylib::ClearBackground(raylib::Color{40, 50, 40, 255});
  TestLayout t;

  auto &root = t.make_ui(pixels(RENDER_W), pixels(RENDER_H));
  t.ui(root).set_flex_direction(FlexDirection::Row);
  t.ui(root).flex_wrap = FlexWrap::Wrap;
  t.ui(root).desired_gap = pixels(10);
  t.ui(root).set_desired_padding(pixels(15), Axis::X);
  t.ui(root).set_desired_padding(pixels(15), Axis::Y);

  // Enough items to wrap
  for (int i = 0; i < 12; i++) {
    auto &item = t.make_ui(pixels(150), pixels(100));
    t.add_child(root, item);
  }

  t.run(root);
  draw_layout_tree(t, root);
}

static void scene_min_max_constraints() {
  raylib::ClearBackground(raylib::Color{50, 45, 55, 255});
  TestLayout t;

  auto &root = t.make_ui(pixels(RENDER_W), pixels(RENDER_H));
  t.ui(root).set_flex_direction(FlexDirection::Row);
  t.ui(root).set_desired_padding(pixels(20), Axis::X);
  t.ui(root).set_desired_padding(pixels(20), Axis::Y);

  // Item with min width -- won't shrink below 200
  auto &a = t.make_ui(expand(1), percent(1.0f));
  t.ui(a).set_min_width(pixels(200));
  t.add_child(root, a);

  // Item with max width -- won't grow above 300
  auto &b = t.make_ui(expand(2), percent(1.0f));
  t.ui(b).set_max_width(pixels(300));
  t.add_child(root, b);

  // Unconstrained
  auto &c = t.make_ui(expand(1), percent(1.0f));
  t.add_child(root, c);

  t.run(root);
  draw_layout_tree(t, root);
}

static void scene_absolute_positioning() {
  raylib::ClearBackground(raylib::Color{60, 60, 60, 255});
  TestLayout t;

  auto &root = t.make_ui(pixels(RENDER_W), pixels(RENDER_H));
  t.ui(root).set_flex_direction(FlexDirection::Column);
  t.ui(root).set_desired_padding(pixels(20), Axis::X);
  t.ui(root).set_desired_padding(pixels(20), Axis::Y);

  // Flow children
  auto &flow1 = t.make_ui(percent(1.0f), pixels(150));
  auto &flow2 = t.make_ui(percent(1.0f), pixels(150));
  t.add_child(root, flow1);
  t.add_child(root, flow2);

  // Absolute child -- overlaps flow children
  auto &abs1 = t.make_ui(pixels(200), pixels(200));
  t.ui(abs1).absolute = true;
  t.ui(abs1).set_desired_margin(pixels(100), Axis::left);
  t.ui(abs1).set_desired_margin(pixels(80), Axis::top);
  t.add_child(root, abs1);

  t.run(root);
  draw_layout_tree(t, root);
}

static void scene_holy_grail() {
  raylib::ClearBackground(raylib::Color{30, 30, 30, 255});
  TestLayout t;

  auto &root = t.make_ui(pixels(RENDER_W), pixels(RENDER_H));
  t.ui(root).set_flex_direction(FlexDirection::Column);

  // Header
  auto &header = t.make_ui(percent(1.0f), pixels(60));
  t.add_child(root, header);

  // Middle row: nav + main + aside
  auto &middle = t.make_ui(percent(1.0f), expand(1));
  t.ui(middle).set_flex_direction(FlexDirection::Row);
  t.add_child(root, middle);

  auto &nav = t.make_ui(pixels(150), percent(1.0f));
  auto &main_area = t.make_ui(expand(1), percent(1.0f));
  auto &aside = t.make_ui(pixels(200), percent(1.0f));
  t.add_child(middle, nav);
  t.add_child(middle, main_area);
  t.add_child(middle, aside);

  // Main area has some children
  t.ui(main_area).set_flex_direction(FlexDirection::Column);
  t.ui(main_area).set_desired_padding(pixels(10), Axis::X);
  t.ui(main_area).set_desired_padding(pixels(10), Axis::Y);
  for (int i = 0; i < 3; i++) {
    auto &card = t.make_ui(percent(1.0f), pixels(80));
    t.add_child(main_area, card);
  }

  // Footer
  auto &footer = t.make_ui(percent(1.0f), pixels(40));
  t.add_child(root, footer);

  t.run(root);
  draw_layout_tree(t, root);
}

static void scene_sidebar_layout() {
  raylib::ClearBackground(raylib::Color{35, 35, 45, 255});
  TestLayout t;

  auto &root = t.make_ui(pixels(RENDER_W), pixels(RENDER_H));
  t.ui(root).set_flex_direction(FlexDirection::Row);

  // Fixed sidebar
  auto &sidebar = t.make_ui(pixels(250), percent(1.0f));
  t.ui(sidebar).set_flex_direction(FlexDirection::Column);
  t.ui(sidebar).set_desired_padding(pixels(8), Axis::X);
  t.ui(sidebar).set_desired_padding(pixels(8), Axis::Y);
  t.add_child(root, sidebar);

  for (int i = 0; i < 5; i++) {
    auto &item = t.make_ui(percent(1.0f), pixels(40));
    t.add_child(sidebar, item);
  }

  // Expanding content area
  auto &content = t.make_ui(expand(1), percent(1.0f));
  t.ui(content).set_desired_padding(pixels(15), Axis::X);
  t.ui(content).set_desired_padding(pixels(15), Axis::Y);
  t.ui(content).set_flex_direction(FlexDirection::Column);
  t.add_child(root, content);

  for (int i = 0; i < 4; i++) {
    auto &row = t.make_ui(percent(1.0f), pixels(100));
    t.add_child(content, row);
  }

  t.run(root);
  draw_layout_tree(t, root);
}

static void scene_card_grid() {
  raylib::ClearBackground(raylib::Color{45, 40, 50, 255});
  TestLayout t;

  auto &root = t.make_ui(pixels(RENDER_W), pixels(RENDER_H));
  t.ui(root).set_flex_direction(FlexDirection::Row);
  t.ui(root).flex_wrap = FlexWrap::Wrap;
  t.ui(root).desired_gap = pixels(12);
  t.ui(root).set_desired_padding(pixels(20), Axis::X);
  t.ui(root).set_desired_padding(pixels(20), Axis::Y);

  for (int i = 0; i < 9; i++) {
    auto &card = t.make_ui(pixels(220), pixels(160));
    t.add_child(root, card);
  }

  t.run(root);
  draw_layout_tree(t, root);
}

// -- Tier 3: Visual components -----------------------------------------------

static void scene_borders() {
  raylib::ClearBackground(raylib::Color{240, 240, 240, 255});

  // Thin border
  draw_rectangle(raylib::Rectangle{50, 50, 200, 120},
                 raylib::Color{200, 220, 240, 255});
  draw_rectangle_outline(raylib::Rectangle{50, 50, 200, 120},
                         raylib::Color{60, 100, 180, 255}, 1.0f);

  // Medium border
  draw_rectangle(raylib::Rectangle{300, 50, 200, 120},
                 raylib::Color{240, 220, 200, 255});
  draw_rectangle_outline(raylib::Rectangle{300, 50, 200, 120},
                         raylib::Color{180, 100, 60, 255}, 3.0f);

  // Thick border
  draw_rectangle(raylib::Rectangle{550, 50, 200, 120},
                 raylib::Color{220, 240, 220, 255});
  draw_rectangle_outline(raylib::Rectangle{550, 50, 200, 120},
                         raylib::Color{60, 140, 60, 255}, 5.0f);

  // Rounded with border
  draw_rectangle_rounded(raylib::Rectangle{50, 250, 300, 150}, 0.2f, 8,
                         raylib::Color{230, 230, 250, 255},
                         std::bitset<4>("1111"));
  draw_rectangle_rounded_lines(raylib::Rectangle{50, 250, 300, 150}, 0.2f, 8,
                               raylib::Color{80, 80, 180, 255},
                               std::bitset<4>("1111"));

  // Nested borders
  draw_rectangle(raylib::Rectangle{450, 250, 300, 300},
                 raylib::Color{250, 250, 250, 255});
  draw_rectangle_outline(raylib::Rectangle{450, 250, 300, 300},
                         raylib::Color{100, 100, 100, 255}, 2.0f);
  draw_rectangle(raylib::Rectangle{470, 270, 260, 260},
                 raylib::Color{240, 240, 240, 255});
  draw_rectangle_outline(raylib::Rectangle{470, 270, 260, 260},
                         raylib::Color{150, 150, 150, 255}, 1.0f);
}

static void scene_scissor_clipping() {
  raylib::ClearBackground(raylib::Color{255, 255, 255, 255});

  // Draw a large rectangle, but clip it to a smaller region
  begin_scissor_mode(100, 100, 300, 200);
  draw_rectangle(raylib::Rectangle{50, 50, 500, 400},
                 raylib::Color{255, 100, 100, 255});
  draw_circle(250, 200, 150.0f, raylib::Color{100, 100, 255, 255});
  draw_text("Clipped!", 120, 180, 30, raylib::Color{255, 255, 255, 255});
  end_scissor_mode();

  // Unclipped reference
  draw_rectangle_outline(raylib::Rectangle{100, 100, 300, 200},
                         raylib::Color{0, 0, 0, 255}, 2.0f);

  // Second clipping region
  begin_scissor_mode(500, 200, 250, 300);
  for (int i = 0; i < 10; i++) {
    draw_rectangle(
        raylib::Rectangle{480.0f + i * 30.0f, 180.0f + i * 40.0f, 60, 60},
        raylib::Color{(unsigned char)(50 + i * 20), 150,
                      (unsigned char)(200 - i * 15), 255});
  }
  end_scissor_mode();

  draw_rectangle_outline(raylib::Rectangle{500, 200, 250, 300},
                         raylib::Color{0, 0, 0, 255}, 2.0f);
}

static void scene_opacity_layers() {
  raylib::ClearBackground(raylib::Color{20, 20, 30, 255});

  // Gradient-like opacity layers
  for (int i = 0; i < 10; i++) {
    unsigned char alpha = (unsigned char)(25 + i * 23);
    draw_rectangle(
        raylib::Rectangle{50.0f + i * 20.0f, 50.0f + i * 20.0f, 300, 200},
        raylib::Color{200, 200, 255, alpha});
  }

  // Overlapping circles with varying opacity
  for (int i = 0; i < 8; i++) {
    unsigned char alpha = (unsigned char)(30 + i * 28);
    draw_circle(500 + (i % 4) * 40, 200 + (i / 4) * 60, 50.0f,
                raylib::Color{255, (unsigned char)(100 + i * 15), 50, alpha});
  }
}

// -- Tier 4: Edge cases ------------------------------------------------------

static void scene_subpixel_positions() {
  raylib::ClearBackground(raylib::Color{128, 128, 128, 255});

  // Rectangles at subpixel positions
  draw_rectangle(raylib::Rectangle{10.3f, 10.7f, 100.5f, 50.2f},
                 raylib::Color{255, 0, 0, 255});
  draw_rectangle(raylib::Rectangle{10.0f, 80.0f, 100.0f, 50.0f},
                 raylib::Color{0, 255, 0, 255}); // integer reference
  draw_rectangle(raylib::Rectangle{10.9f, 150.1f, 99.1f, 50.9f},
                 raylib::Color{0, 0, 255, 255});

  // Lines at subpixel positions
  draw_line_ex(raylib::Vector2{200.3f, 10.7f}, raylib::Vector2{400.8f, 10.2f},
               1.0f, raylib::Color{255, 255, 0, 255});
  draw_line_ex(raylib::Vector2{200.0f, 30.0f}, raylib::Vector2{400.0f, 30.0f},
               1.0f, raylib::Color{255, 255, 255, 255}); // integer reference

  // Circles at subpixel centers
  draw_circle(500, 100, 30.3f, raylib::Color{200, 100, 255, 255});
  draw_circle(600, 100, 30.0f,
              raylib::Color{100, 200, 255, 255}); // integer reference
}

static void scene_screen_edges() {
  raylib::ClearBackground(raylib::Color{200, 200, 200, 255});

  // Rectangles touching all four edges
  draw_rectangle(raylib::Rectangle{0, 0, 50, 50},
                 raylib::Color{255, 0, 0, 255}); // top-left
  draw_rectangle(raylib::Rectangle{(float)(RENDER_W - 50), 0, 50, 50},
                 raylib::Color{0, 255, 0, 255}); // top-right
  draw_rectangle(raylib::Rectangle{0, (float)(RENDER_H - 50), 50, 50},
                 raylib::Color{0, 0, 255, 255}); // bottom-left
  draw_rectangle(
      raylib::Rectangle{(float)(RENDER_W - 50), (float)(RENDER_H - 50), 50,
                         50},
      raylib::Color{255, 255, 0, 255}); // bottom-right

  // Full-width/height rectangles
  draw_rectangle(raylib::Rectangle{0, 270, (float)RENDER_W, 4},
                 raylib::Color{0, 0, 0, 255}); // horizontal line
  draw_rectangle(raylib::Rectangle{398, 0, 4, (float)RENDER_H},
                 raylib::Color{0, 0, 0, 255}); // vertical line

  // Partially off-screen (negative coords / exceeding bounds)
  draw_rectangle(raylib::Rectangle{-25, 200, 50, 50},
                 raylib::Color{255, 0, 255, 255});
  draw_rectangle(raylib::Rectangle{(float)(RENDER_W - 25), 200, 50, 50},
                 raylib::Color{0, 255, 255, 255});
}

static void scene_tiny_elements() {
  raylib::ClearBackground(raylib::Color{180, 180, 180, 255});

  // 1x1 pixel
  draw_rectangle(raylib::Rectangle{100, 100, 1, 1},
                 raylib::Color{255, 0, 0, 255});
  // 2x2
  draw_rectangle(raylib::Rectangle{110, 100, 2, 2},
                 raylib::Color{0, 255, 0, 255});
  // 1px wide line
  draw_rectangle(raylib::Rectangle{120, 100, 1, 100},
                 raylib::Color{0, 0, 255, 255});
  // 1px tall line
  draw_rectangle(raylib::Rectangle{130, 100, 100, 1},
                 raylib::Color{255, 0, 255, 255});

  // Tiny circles
  draw_circle(300, 100, 1.0f, raylib::Color{255, 0, 0, 255});
  draw_circle(320, 100, 2.0f, raylib::Color{0, 255, 0, 255});
  draw_circle(340, 100, 0.5f, raylib::Color{0, 0, 255, 255});

  // Many small rects in a grid
  for (int y = 0; y < 20; y++) {
    for (int x = 0; x < 40; x++) {
      unsigned char r = (unsigned char)(x * 6);
      unsigned char g = (unsigned char)(y * 12);
      draw_rectangle(
          raylib::Rectangle{50.0f + x * 15.0f, 250.0f + y * 15.0f, 10, 10},
          raylib::Color{r, g, 128, 255});
    }
  }
}

static void scene_deep_nesting() {
  raylib::ClearBackground(raylib::Color{40, 40, 40, 255});
  TestLayout t;

  auto &root = t.make_ui(pixels(RENDER_W), pixels(RENDER_H));
  t.ui(root).set_flex_direction(FlexDirection::Column);
  t.ui(root).set_desired_padding(pixels(10), Axis::X);
  t.ui(root).set_desired_padding(pixels(10), Axis::Y);

  // 8 levels of nesting, alternating Row/Column
  Entity *current = &root;
  for (int depth = 0; depth < 8; depth++) {
    auto &child = t.make_ui(percent(1.0f), percent(1.0f));
    FlexDirection dir =
        (depth % 2 == 0) ? FlexDirection::Row : FlexDirection::Column;
    t.ui(child).set_flex_direction(dir);
    t.ui(child).set_desired_padding(pixels(8), Axis::X);
    t.ui(child).set_desired_padding(pixels(8), Axis::Y);

    // Add a sibling at each level to make it non-trivial
    auto &sibling = t.make_ui(pixels(30), percent(1.0f));
    t.add_child(*current, sibling);
    t.add_child(*current, child);
    current = &child;
  }

  // Leaf
  auto &leaf = t.make_ui(percent(1.0f), percent(1.0f));
  t.add_child(*current, leaf);

  t.run(root);
  draw_layout_tree(t, root);
}

static void scene_polygons() {
  raylib::ClearBackground(raylib::Color{25, 25, 40, 255});

  // Pentagon
  draw_poly(raylib::Vector2{150, 150}, 5, 80.0f, 0.0f,
            raylib::Color{200, 100, 100, 255});
  // Hexagon
  draw_poly(raylib::Vector2{350, 150}, 6, 80.0f, 30.0f,
            raylib::Color{100, 200, 100, 255});
  // Octagon
  draw_poly(raylib::Vector2{550, 150}, 8, 80.0f, 22.5f,
            raylib::Color{100, 100, 200, 255});
  // Triangle (polygon)
  draw_poly(raylib::Vector2{150, 400}, 3, 80.0f, 0.0f,
            raylib::Color{230, 200, 80, 255});

  // Outlined polygons
  draw_poly_lines(raylib::Vector2{350, 400}, 5, 80.0f, 0.0f,
                  raylib::Color{255, 255, 255, 255});
  draw_poly_lines_ex(raylib::Vector2{550, 400}, 6, 80.0f, 0.0f, 3.0f,
                     raylib::Color{255, 200, 100, 255});

  // Ring segments
  draw_ring_segment(700, 150, 30.0f, 60.0f, 0.0f, 270.0f, 16,
                    raylib::Color{200, 150, 255, 255});
  draw_ring(700, 400, 30.0f, 60.0f, 24,
            raylib::Color{150, 255, 200, 255});
}

static void scene_mixed_primitives_and_layout() {
  raylib::ClearBackground(raylib::Color{35, 35, 45, 255});

  // Layout-driven background panels
  TestLayout t;
  auto &root = t.make_ui(pixels(RENDER_W), pixels(RENDER_H));
  t.ui(root).set_flex_direction(FlexDirection::Column);
  t.ui(root).set_desired_padding(pixels(20), Axis::X);
  t.ui(root).set_desired_padding(pixels(20), Axis::Y);

  auto &top = t.make_ui(percent(1.0f), pixels(200));
  auto &bottom = t.make_ui(percent(1.0f), expand(1));
  t.ui(bottom).set_flex_direction(FlexDirection::Row);
  t.add_child(root, top);
  t.add_child(root, bottom);

  auto &left = t.make_ui(pixels(300), percent(1.0f));
  auto &right = t.make_ui(expand(1), percent(1.0f));
  t.add_child(bottom, left);
  t.add_child(bottom, right);

  t.run(root);
  draw_layout_tree(t, root);

  // Overlay primitives on top of layout
  draw_circle(400, 300, 50.0f, raylib::Color{255, 255, 100, 150});
  draw_line_ex(raylib::Vector2{0, 0}, raylib::Vector2{800, 600}, 2.0f,
               raylib::Color{255, 255, 255, 80});
  draw_line_ex(raylib::Vector2{800, 0}, raylib::Vector2{0, 600}, 2.0f,
               raylib::Color{255, 255, 255, 80});
  draw_text("Overlay", 330, 280, 24, raylib::Color{255, 255, 255, 200});
}

static void scene_bevel_borders() {
  raylib::ClearBackground(raylib::Color{192, 192, 192, 255});

  auto draw_bevel = [](float x, float y, float w, float h, int thickness,
                       bool raised) {
    raylib::Color light = raised ? raylib::Color{240, 240, 240, 255}
                                 : raylib::Color{80, 80, 80, 255};
    raylib::Color dark = raised ? raylib::Color{80, 80, 80, 255}
                                : raylib::Color{240, 240, 240, 255};
    raylib::Color face{192, 192, 192, 255};

    draw_rectangle(raylib::Rectangle{x, y, w, h}, face);
    for (int i = 0; i < thickness; i++) {
      float fi = static_cast<float>(i);
      // Top edge
      draw_line(static_cast<int>(x + fi), static_cast<int>(y + fi),
                static_cast<int>(x + w - fi), static_cast<int>(y + fi), light);
      // Left edge
      draw_line(static_cast<int>(x + fi), static_cast<int>(y + fi),
                static_cast<int>(x + fi), static_cast<int>(y + h - fi), light);
      // Bottom edge
      draw_line(static_cast<int>(x + fi), static_cast<int>(y + h - fi),
                static_cast<int>(x + w - fi), static_cast<int>(y + h - fi),
                dark);
      // Right edge
      draw_line(static_cast<int>(x + w - fi), static_cast<int>(y + fi),
                static_cast<int>(x + w - fi), static_cast<int>(y + h - fi),
                dark);
    }
  };

  draw_bevel(50, 50, 200, 100, 2, true);
  draw_text("Raised", 110, 90, 20, raylib::Color{0, 0, 0, 255});

  draw_bevel(300, 50, 200, 100, 2, false);
  draw_text("Sunken", 360, 90, 20, raylib::Color{0, 0, 0, 255});

  draw_bevel(50, 200, 300, 60, 3, true);
  draw_bevel(550, 200, 200, 200, 4, false);

  draw_bevel(50, 310, 450, 250, 2, true);
  draw_bevel(65, 325, 420, 220, 2, false);
  draw_text("Nested bevel", 180, 420, 24, raylib::Color{0, 0, 0, 255});
}

static void scene_shadows() {
  raylib::ClearBackground(raylib::Color{245, 245, 245, 255});

  auto draw_shadow_rect = [](float x, float y, float w, float h, float offset,
                             unsigned char shadow_alpha) {
    draw_rectangle(raylib::Rectangle{x + offset, y + offset, w, h},
                   raylib::Color{0, 0, 0, shadow_alpha});
    draw_rectangle(raylib::Rectangle{x, y, w, h},
                   raylib::Color{255, 255, 255, 255});
    draw_rectangle_outline(raylib::Rectangle{x, y, w, h},
                           raylib::Color{200, 200, 200, 255}, 1.0f);
  };

  draw_shadow_rect(50, 50, 180, 120, 3, 40);
  draw_text("Light", 100, 100, 20, raylib::Color{80, 80, 80, 255});

  draw_shadow_rect(300, 50, 180, 120, 5, 80);
  draw_text("Medium", 345, 100, 20, raylib::Color{80, 80, 80, 255});

  draw_shadow_rect(550, 50, 180, 120, 8, 120);
  draw_text("Heavy", 600, 100, 20, raylib::Color{80, 80, 80, 255});

  draw_shadow_rect(100, 250, 250, 200, 4, 60);
  draw_shadow_rect(500, 250, 200, 300, 6, 90);

  draw_shadow_rect(120, 280, 100, 80, 2, 30);
}

static void scene_text_sizes() {
  raylib::ClearBackground(raylib::Color{255, 255, 255, 255});

  const int sizes[] = {8, 10, 12, 14, 16, 20, 24, 32, 48, 64};
  float y = 20;
  for (int size : sizes) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%dpx: The quick brown fox", size);
    draw_text(buf, 20, static_cast<int>(y), size,
              raylib::Color{30, 30, 30, 255});
    y += size + 8;
  }
}

static void scene_color_spectrum() {
  raylib::ClearBackground(raylib::Color{0, 0, 0, 255});

  for (int x = 0; x < RENDER_W; x++) {
    for (int y = 0; y < 200; y++) {
      unsigned char r = static_cast<unsigned char>(x * 255 / RENDER_W);
      unsigned char g = static_cast<unsigned char>(y * 255 / 200);
      unsigned char b = 128;
      draw_rectangle(raylib::Rectangle{(float)x, (float)y, 1, 1},
                     raylib::Color{r, g, b, 255});
    }
  }

  for (int x = 0; x < RENDER_W; x++) {
    unsigned char v = static_cast<unsigned char>(x * 255 / RENDER_W);
    draw_rectangle(raylib::Rectangle{(float)x, 220, 1, 40},
                   raylib::Color{v, 0, 0, 255});
    draw_rectangle(raylib::Rectangle{(float)x, 270, 1, 40},
                   raylib::Color{0, v, 0, 255});
    draw_rectangle(raylib::Rectangle{(float)x, 320, 1, 40},
                   raylib::Color{0, 0, v, 255});
    draw_rectangle(raylib::Rectangle{(float)x, 370, 1, 40},
                   raylib::Color{v, v, v, 255});
  }

  for (int x = 0; x < RENDER_W; x++) {
    unsigned char a = static_cast<unsigned char>(x * 255 / RENDER_W);
    draw_rectangle(raylib::Rectangle{(float)x, 430, 1, 40},
                   raylib::Color{255, 100, 50, a});
  }
}

// ============================================================================
// Scene registry
// ============================================================================

static std::vector<Scene> build_scene_list() {
  return {
      // Tier 1: Primitives
      {"filled_rects", scene_filled_rects},
      {"outlined_rects", scene_outlined_rects},
      {"rounded_rects", scene_rounded_rects},
      {"circles_lines_triangles", scene_circles_lines_triangles},
      {"text", scene_text},
      {"alpha_blending", scene_alpha_blending},
      {"polygons", scene_polygons},
      // Tier 2: Layout
      {"flex_column", scene_flex_column},
      {"flex_row", scene_flex_row},
      {"flex_grow_weighted", scene_flex_grow_weighted},
      {"justify_content", scene_justify_content},
      {"align_items", scene_align_items},
      {"wrap_with_gap", scene_wrap_with_gap},
      {"min_max_constraints", scene_min_max_constraints},
      {"absolute_positioning", scene_absolute_positioning},
      {"holy_grail", scene_holy_grail},
      {"sidebar_layout", scene_sidebar_layout},
      {"card_grid", scene_card_grid},
      // Tier 3: Visual components
      {"borders", scene_borders},
      {"bevel_borders", scene_bevel_borders},
      {"shadows", scene_shadows},
      {"scissor_clipping", scene_scissor_clipping},
      {"opacity_layers", scene_opacity_layers},
      {"text_sizes", scene_text_sizes},
      {"color_spectrum", scene_color_spectrum},
      // Tier 4: Edge cases
      {"subpixel_positions", scene_subpixel_positions},
      {"screen_edges", scene_screen_edges},
      {"tiny_elements", scene_tiny_elements},
      {"deep_nesting", scene_deep_nesting},
      {"mixed_primitives_and_layout", scene_mixed_primitives_and_layout},
  };
}

// ============================================================================
// Image comparison
// ============================================================================

struct CompareResult {
  bool match = false;
  int first_mismatch_x = -1;
  int first_mismatch_y = -1;
  int total_mismatches = 0;
  int total_pixels = 0;
  int max_channel_diff = 0;
  double mismatch_pct = 0.0;
};

static CompareResult compare_and_diff(const std::filesystem::path &path_a,
                                      const std::filesystem::path &path_b,
                                      const std::filesystem::path &diff_path,
                                      int tolerance = 0) {
  raylib::Image a = raylib::LoadImage(path_a.c_str());
  raylib::Image b = raylib::LoadImage(path_b.c_str());

  if (a.data == nullptr || b.data == nullptr) {
    if (a.data)
      raylib::UnloadImage(a);
    if (b.data)
      raylib::UnloadImage(b);
    return {false, 0, 0, -1, 0, 0, 0.0};
  }

  if (a.width != b.width || a.height != b.height) {
    fprintf(stderr, "    dimension mismatch: %dx%d vs %dx%d\n", a.width,
            a.height, b.width, b.height);
    raylib::UnloadImage(a);
    raylib::UnloadImage(b);
    return {false, 0, 0, -1, 0, 0, 0.0};
  }

  int pixel_count = a.width * a.height;
  int bytes = pixel_count * 4; // RGBA

  // Fast path: exact match
  if (memcmp(a.data, b.data, bytes) == 0) {
    raylib::UnloadImage(a);
    raylib::UnloadImage(b);
    return {true, 0, 0, 0, pixel_count, 0, 0.0};
  }

  CompareResult result;
  result.total_pixels = pixel_count;

  raylib::Image diff = raylib::GenImageColor(a.width, a.height,
                                             raylib::Color{255, 255, 255, 255});

  const auto *pa = static_cast<const unsigned char *>(a.data);
  const auto *pb = static_cast<const unsigned char *>(b.data);

  for (int i = 0; i < pixel_count; i++) {
    int offset = i * 4;
    int dr = abs((int)pa[offset] - (int)pb[offset]);
    int dg = abs((int)pa[offset + 1] - (int)pb[offset + 1]);
    int db = abs((int)pa[offset + 2] - (int)pb[offset + 2]);
    int da = abs((int)pa[offset + 3] - (int)pb[offset + 3]);
    int channel_max = std::max({dr, dg, db, da});

    if (channel_max > tolerance) {
      result.total_mismatches++;
      result.max_channel_diff = std::max(result.max_channel_diff, channel_max);
      int x = i % a.width;
      int y = i / a.width;
      if (result.first_mismatch_x < 0) {
        result.first_mismatch_x = x;
        result.first_mismatch_y = y;
      }
      unsigned char intensity = static_cast<unsigned char>(
          std::min(255, channel_max * 255 / std::max(1, 255 - tolerance)));
      raylib::ImageDrawPixel(&diff, x, y,
                             raylib::Color{intensity, 0, 0, 255});
    }
  }

  result.mismatch_pct =
      100.0 * result.total_mismatches / std::max(1, pixel_count);
  result.match = (result.total_mismatches == 0);

  if (!result.match) {
    raylib::ExportImage(diff, diff_path.c_str());
  }

  raylib::UnloadImage(diff);
  raylib::UnloadImage(a);
  raylib::UnloadImage(b);

  return result;
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char *argv[]) {
  std::string scene_filter;
  std::filesystem::path output_dir = "headless_validation_output";
  int tolerance = 0;
  bool verbose = false;
  bool quiet = false;
  bool list_scenes = false;

  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg.starts_with("--scene=")) {
      scene_filter = arg.substr(8);
    } else if (arg.starts_with("--output-dir=")) {
      output_dir = arg.substr(13);
    } else if (arg.starts_with("--tolerance=")) {
      tolerance = std::atoi(arg.substr(12).c_str());
    } else if (arg == "--verbose" || arg == "-v") {
      verbose = true;
    } else if (arg == "--quiet" || arg == "-q") {
      quiet = true;
    } else if (arg == "--list") {
      list_scenes = true;
    } else if (arg == "--help" || arg == "-h") {
      printf("Usage: %s [options]\n", argv[0]);
      printf("  --scene=NAME        Run only the named scene\n");
      printf("  --output-dir=PATH   Output directory (default: "
             "headless_validation_output)\n");
      printf("  --tolerance=N       Max per-channel diff to ignore (0=exact, "
             "default: 0)\n");
      printf("  --verbose, -v       Show detailed per-scene info\n");
      printf("  --quiet, -q         Suppress raylib INFO logs\n");
      printf("  --list              List available scenes and exit\n");
      printf("  --help, -h          Show this help\n");
      return 0;
    }
  }

  if (list_scenes) {
    auto scenes = build_scene_list();
    printf("Available scenes (%zu):\n", scenes.size());
    for (const auto &s : scenes)
      printf("  %s\n", s.name);
    return 0;
  }

  if (quiet) {
    raylib::SetTraceLogLevel(raylib::LOG_WARNING);
  }

  std::filesystem::create_directories(output_dir);

  auto scenes = build_scene_list();
  int pass = 0;
  int fail = 0;
  int skip = 0;

  printf("=== Headless Rendering Validation ===\n");
  printf("Resolution:  %dx%d\n", RENDER_W, RENDER_H);
  printf("Tolerance:   %d (per-channel)\n", tolerance);
  printf("Output dir:  %s\n", output_dir.c_str());
  printf("==============================\n");

  // Filter scenes
  std::vector<Scene *> active_scenes;
  for (auto &scene : scenes) {
    if (!scene_filter.empty() && scene_filter != scene.name) {
      skip++;
      continue;
    }
    active_scenes.push_back(&scene);
  }

  // Render all headless scenes in one backend session to avoid state
  // corruption from repeated init/shutdown cycles.
  {
    graphics::Config cfg{};
    cfg.display = graphics::DisplayMode::Headless;
    cfg.width = RENDER_W;
    cfg.height = RENDER_H;
    cfg.target_fps = 60;

    if (!graphics::init(cfg)) {
      fprintf(stderr, "FATAL: headless backend init failed\n");
      return 1;
    }

    for (auto *scene : active_scenes) {
      auto path = output_dir / (std::string(scene->name) + "_headless.png");
      graphics::begin_frame();
      scene->render();
      graphics::end_frame();
      graphics::capture_frame(path);
    }

    graphics::shutdown();
  }

  // Render all windowed scenes in one backend session.
  {
    graphics::Config cfg{};
    cfg.display = graphics::DisplayMode::Windowed;
    cfg.width = RENDER_W;
    cfg.height = RENDER_H;
    cfg.target_fps = 60;
    cfg.config_flags = raylib::FLAG_WINDOW_HIDDEN;
    cfg.enable_msaa = false;

    if (!graphics::init(cfg)) {
      fprintf(stderr, "FATAL: windowed backend init failed\n");
      return 1;
    }

    for (auto *scene : active_scenes) {
      auto path = output_dir / (std::string(scene->name) + "_windowed.png");
      graphics::begin_frame();
      scene->render();
      graphics::end_frame();
      graphics::capture_frame(path);
    }

    graphics::shutdown();
  }

  // Compare all scenes
  for (auto *scene : active_scenes) {
    auto headless_path =
        output_dir / (std::string(scene->name) + "_headless.png");
    auto windowed_path =
        output_dir / (std::string(scene->name) + "_windowed.png");
    auto diff_path = output_dir / (std::string(scene->name) + "_diff.png");

    auto result =
        compare_and_diff(headless_path, windowed_path, diff_path, tolerance);
    if (result.match) {
      printf("  PASS  %-35s", scene->name);
      if (verbose)
        printf(" (%d pixels)", result.total_pixels);
      printf("\n");
      pass++;
    } else {
      printf("  FAIL  %-35s %d px (%.2f%%) differ, max=%d, first=(%d,%d)\n",
             scene->name, result.total_mismatches, result.mismatch_pct,
             result.max_channel_diff, result.first_mismatch_x,
             result.first_mismatch_y);
      fail++;
    }
  }

  printf("==============================\n");
  printf("%d/%d passed", pass, pass + fail);
  if (skip > 0)
    printf(", %d skipped", skip);
  if (tolerance > 0)
    printf(" (tolerance=%d)", tolerance);
  printf("\n");

  if (fail > 0) {
    printf("Diff images saved to: %s\n", output_dir.c_str());
  }

  return fail > 0 ? 1 : 0;
}
