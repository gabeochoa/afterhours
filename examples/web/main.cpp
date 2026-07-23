// web/main.cpp
// Graphical afterhours demo using the SOKOL backend, built for the web
// (WebAssembly + WebGL2) with Emscripten. Sokol targets WebGL natively, so the
// web build is a single emcc command (see Makefile / README.md).
//
// A small ECS showcase: entities carry Transform / Tint / Pulse components and
// are driven by several systems (movement, size pulsing, color cycling) plus a
// dedicated render system. The SOKOL backend owns the window and main loop and
// calls our frame callback.

#include <cmath>
#include <cstdlib>

#define AFTER_HOURS_ENTITY_HELPER
#define AFTER_HOURS_ENTITY_QUERY
#define AFTER_HOURS_SYSTEM
#define AFTERHOURS_SINGLE_RENDER_PASS // render systems run once per frame
#define AFTER_HOURS_USE_METAL         // selects the sokol backend

#include <afterhours/ah.h>
#include <afterhours/src/drawing_helpers.h>
#include <afterhours/src/graphics.h>
#include <afterhours/src/plugins/texture_manager.h>

using namespace afterhours;
namespace g = afterhours::graphics;
namespace tex = afterhours::texture_manager;

constexpr int WIDTH = 800;
constexpr int HEIGHT = 600;

// ---- Components -------------------------------------------------------------
struct Transform : BaseComponent {
  float x = 0, y = 0;
  float vx = 0, vy = 0;
  float size = 30;
};
struct Tint : BaseComponent {
  float hue = 0;      // degrees, cycled over time
  Color color{255, 255, 255, 255};
};
struct Pulse : BaseComponent {
  float base = 30;    // resting size
  float amp = 6;      // how much the size breathes
  float phase = 0;    // current phase
  float speed = 2;    // radians / second
};

// ---- Systems ----------------------------------------------------------------
// Move each box and bounce it off the window edges.
struct MovementSystem : System<Transform> {
  void for_each_with(Entity &, Transform &t, float dt) override {
    const float w = static_cast<float>(g::get_screen_width());
    const float h = static_cast<float>(g::get_screen_height());
    t.x += t.vx * dt;
    t.y += t.vy * dt;
    if (t.x < 0 || t.x + t.size > w)
      t.vx *= -1.f;
    if (t.y < 0 || t.y + t.size > h)
      t.vy *= -1.f;
  }
};

// Breathe each box's size in and out.
struct PulseSystem : System<Transform, Pulse> {
  void for_each_with(Entity &, Transform &t, Pulse &p, float dt) override {
    p.phase += p.speed * dt;
    t.size = p.base + p.amp * std::sin(p.phase);
  }
};

// Slowly rotate each box's hue.
struct ColorCycleSystem : System<Tint> {
  static Color from_hsv(float h, float s, float v) {
    const float c = v * s;
    const float x = c * (1.f - std::fabs(std::fmod(h / 60.f, 2.f) - 1.f));
    const float m = v - c;
    float r = 0, gr = 0, b = 0;
    if (h < 60) { r = c; gr = x; }
    else if (h < 120) { r = x; gr = c; }
    else if (h < 180) { gr = c; b = x; }
    else if (h < 240) { gr = x; b = c; }
    else if (h < 300) { r = x; b = c; }
    else { r = c; b = x; }
    auto to8 = [](float f) { return static_cast<unsigned char>((f) * 255.f); };
    return Color{to8(r + m), to8(gr + m), to8(b + m), 255};
  }
  void for_each_with(Entity &, Tint &tint, float dt) override {
    tint.hue = std::fmod(tint.hue + 40.f * dt, 360.f);
    tint.color = from_hsv(tint.hue, 0.65f, 0.95f);
  }
};

// Draw each box. Registered as a render system so it runs after the updates.
struct RenderSystem : System<Transform, Tint> {
  void for_each_with(Entity &, Transform &t, Tint &tint, float) override {
    draw_rectangle(RectangleType{t.x, t.y, t.size, t.size}, tint.color);
  }
};

// ---- Primitive showcase (sokol geometry primitives) ------------------------
// A static reference scene that exercises every geometry primitive implemented
// in the sokol backend (draw_circle_sector / _lines, draw_ellipse / _lines,
// draw_poly / _lines / _lines_ex, draw_ring / draw_ring_segment). Handy for
// eyeballing the sokol backend against the raylib reference. Registered as a
// render system so its sgl calls land inside the frame's draw pass.
struct PrimitiveShowcaseSystem : System<> {
  void once(float) override {
    const Color white{255, 255, 255, 255};
    const Color red{230, 60, 60, 255};
    const Color green{60, 200, 90, 255};
    const Color blue{70, 130, 240, 255};
    const Color yellow{240, 210, 60, 255};
    const Color purple{180, 90, 220, 255};
    const Color orange{240, 150, 50, 255};
    const Color cyan{60, 210, 220, 255};

    // Row 1
    draw_circle_sector(Vector2Type{120, 120}, 60, 0, 120, 0, red);
    draw_circle_sector_lines(Vector2Type{280, 120}, 60, 200, 320, 0, green);
    draw_ellipse(460, 120, 70, 40, blue);
    draw_ellipse_lines(640, 120, 35, 65, yellow);

    // Row 2
    draw_poly(Vector2Type{120, 340}, 6, 60, 0, purple);
    draw_poly_lines(Vector2Type{300, 340}, 5, 60, -90, orange);
    draw_poly_lines_ex(Vector2Type{480, 340}, 3, 60, -90, 10, cyan);

    // Row 3
    draw_ring(140, 540, 30, 65, 36, white);
    draw_ring_segment(340, 540, 30, 65, 45, 300, 32, red);
    draw_circle(560, 540, 40, green);
    draw_circle_lines(700, 540, 40, blue);
  }
};

static SystemManager systems;

// ---- Texture showcase (sokol texture path) ---------------------------------
// Build a procedural RGBA checker texture (no file IO, AMFI-safe) and draw it
// three ways to exercise the sokol texture backend:
//   - draw_texture_rec   (axis-aligned blit of a sub-rect)
//   - draw_texture_pro   (rotated, scaled blit about an origin)
//   - a sprite via texture_manager (RenderSprites -> draw_texture_pro route)
static TextureType g_checker{};
constexpr int CHECKER_DIM = 64; // 8x8 cells of 8px

static TextureType make_checker_texture() {
  std::vector<unsigned char> px(CHECKER_DIM * CHECKER_DIM * 4);
  for (int y = 0; y < CHECKER_DIM; ++y) {
    for (int x = 0; x < CHECKER_DIM; ++x) {
      const bool on = ((x / 8) + (y / 8)) % 2 == 0;
      unsigned char *p = px.data() + (y * CHECKER_DIM + x) * 4;
      if (on) {
        p[0] = 240; p[1] = 200; p[2] = 60; p[3] = 255; // amber
      } else {
        p[0] = 40; p[1] = 60; p[2] = 120; p[3] = 255;  // slate blue
      }
    }
  }
  return metal_texture_detail::load_texture_from_pixels(px.data(), CHECKER_DIM,
                                                        CHECKER_DIM,
                                                        TEXTURE_FILTER_BILINEAR);
}

// Static reference scene: draws the checker texture via rec + rotated pro.
struct TextureShowcaseSystem : System<> {
  float t = 0.f;
  void once(float dt) override {
    if (g_checker.view_id == 0)
      return;
    t += dt;
    const Color white{255, 255, 255, 255};

    const float tw = g_checker.width;
    const float th = g_checker.height;
    const RectangleType full{0, 0, tw, th};

    // 1) draw_texture_rec: 1:1 blit, top-right corner.
    draw_texture_rec(g_checker, full, Vector2Type{560, 200}, white);

    // 2) draw_texture_pro: scaled 2x and spinning about its center.
    const float scale = 2.0f;
    const RectangleType dest{700, 300, tw * scale, th * scale};
    const Vector2Type origin{tw * scale / 2.f, th * scale / 2.f};
    const float rotation = std::fmod(t * 45.f, 360.f); // deg/sec
    draw_texture_pro(g_checker, full, dest, origin, rotation, white);
  }
};

// ---- Setup ------------------------------------------------------------------
static float frand(float lo, float hi) {
  return lo + (hi - lo) * (static_cast<float>(std::rand()) /
                           static_cast<float>(RAND_MAX));
}

static void spawn_boxes() {
  // Idempotent: if any box already exists, don't spawn again.
  if (EntityQuery<>()
          .whereLambda([](const Entity &e) { return e.has<Transform>(); })
          .has_values())
    return;

  for (int i = 0; i < 24; ++i) {
    auto &e = EntityHelper::createEntity();

    auto &t = e.addComponent<Transform>();
    t.size = frand(18, 46);
    t.x = frand(0, WIDTH - t.size);
    t.y = frand(0, HEIGHT - t.size);
    t.vx = frand(-220, 220);
    t.vy = frand(-220, 220);

    auto &p = e.addComponent<Pulse>();
    p.base = t.size;
    p.amp = frand(3, 9);
    p.phase = frand(0, 6.28f);
    p.speed = frand(1.5f, 3.5f);

    e.addComponent<Tint>().hue = frand(0, 360);
  }
  EntityHelper::merge_entity_arrays();

  // Texture path: build the procedural checker (GPU resources need a live
  // sokol context, which exists by the time cfg.init runs), register it as the
  // sprite-sheet singleton, and spawn one sprite entity so RenderSprites (which
  // routes through the sokol draw_texture_pro) has something to draw.
  g_checker = make_checker_texture();
  {
    auto &sheet = EntityHelper::createEntity();
    sheet.addComponent<tex::HasSpritesheet>(g_checker);

    auto &sprite = EntityHelper::createEntity();
    // Sample the full texture as one "frame"; draw it near bottom-right.
    const tex::Rectangle frame{0, 0, g_checker.width, g_checker.height};
    sprite.addComponent<tex::HasSprite>(
        /*pos*/ Vector2Type{620, 440}, /*size*/ Vector2Type{96, 96},
        /*angle*/ 20.f, frame, /*scale*/ 1.5f,
        tex::Color{255, 255, 255, 255});
    EntityHelper::merge_entity_arrays();
  }
}

// Called once per frame by the sokol backend: update + render systems run
// inside the draw pass so the render system's sgl calls are captured.
static void frame() {
  g::begin_drawing();
  g::clear_background(Color{18, 22, 32, 255});
  systems.run(g::get_frame_time());
  g::end_drawing();
}

int main() {
  std::srand(1234);

  systems.register_update_system(std::make_unique<MovementSystem>());
  systems.register_update_system(std::make_unique<PulseSystem>());
  systems.register_update_system(std::make_unique<ColorCycleSystem>());
  systems.register_render_system(std::make_unique<RenderSystem>());
  // Static reference scene exercising every sokol geometry primitive.
  systems.register_render_system(std::make_unique<PrimitiveShowcaseSystem>());
  // Texture path: direct rec + rotated pro blits.
  systems.register_render_system(std::make_unique<TextureShowcaseSystem>());
  // Sprite path: routes through texture_manager -> sokol draw_texture_pro.
  systems.register_render_system(std::make_unique<tex::RenderSprites>());

  g::RunConfig cfg;
  cfg.width = WIDTH;
  cfg.height = HEIGHT;
  cfg.title = "afterhours + sokol (wasm)";
  cfg.init = spawn_boxes;
  cfg.frame = frame;

  // sokol owns the loop; on the web this drives the emscripten RAF loop.
  g::run(cfg);
  return 0;
}
