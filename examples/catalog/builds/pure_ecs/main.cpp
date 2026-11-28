#include <cstdio>

#include "../../../example_helper.h"

using namespace afterhours;
using namespace afterhours::examples;

struct Position : BaseComponent {
  float x = 0.f;
};
struct Velocity : BaseComponent {
  float dx = 0.f;
};

struct Move : System<Position, Velocity> {
  void for_each_with(Entity &, Position &p, Velocity &v, float dt) override {
    p.x += v.dx * dt;
  }
};

int main() {
  Checks checks;

  auto &ball = EntityHelper::createEntity();
  ball.addComponent<Position>();
  ball.addComponent<Velocity>().dx = 10.f;
  EntityHelper::merge_entity_arrays();

  auto systems = update_systems<Move>();
  run_frames(systems, 60);

  const float x = ball.get<Position>().x;
  std::printf("after 1s the ball is at x=%.2f\n", x);
  checks.near(x, 10.f, 0.1f, "ball moved for one second");
  return checks.finish("pure_ecs");
}
