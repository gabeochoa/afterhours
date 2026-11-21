// minimal_game — the shortest ECS game: one ball, one system, one call.
//
// Everything below main() is the game; main() is game::run plus the three
// callbacks. No window code, no frame loop, no UI setup (that is ui::run).

#include <cstdio>

#include "../../../../ah.h"
#include "../../../../src/drawing_helpers.h"
#include "../../../../src/game.h"

using namespace afterhours;

struct Position : BaseComponent {
  float x = 0.f;
};
struct Velocity : BaseComponent {
  float dx = 0.f;
};

struct Move : System<Position, Velocity> {
  void for_each_with(Entity &, Position &p, Velocity &v, float dt) override {
    p.x += v.dx * dt;
    if (p.x > 300.f || p.x < 0.f) v.dx = -v.dx;
  }
};

int main() {
  SystemManager systems;
  Entity *ball = nullptr;

  game::Config cfg;
  cfg.run.title = "minimal_game";
  cfg.run.width = 320;
  cfg.run.height = 180;
  cfg.run.fixed_dt = 1.f / 60.f;
  cfg.max_frames = 120; // a windowed game would leave this 0 and run to close
  cfg.setup = [&] {
    ball = &EntityHelper::createEntity();
    ball->addComponent<Position>().x = 10.f;
    ball->addComponent<Velocity>().dx = 120.f;
    EntityHelper::merge_entity_arrays();
    systems.register_update_system(std::make_unique<Move>());
  };
  cfg.update = [&](float dt) { systems.run(dt); };
  cfg.draw = [&] {
    draw_rectangle(RectangleType{ball->get<Position>().x, 80.f, 16.f, 16.f},
                   Color{255, 120, 0, 255});
  };

  const int rc = game::run(cfg);
  std::printf("ball ended at x=%.1f\n", ball->get<Position>().x);
  return rc;
}
