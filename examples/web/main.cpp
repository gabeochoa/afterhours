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

using namespace afterhours;
namespace g = afterhours::graphics;

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

static SystemManager systems;

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
