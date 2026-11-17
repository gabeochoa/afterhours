#include <cstdio>

#include "../../../../ah.h"

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
  }
};

int main() {
  auto &ball = EntityHelper::createEntity();
  ball.addComponent<Position>();
  ball.addComponent<Velocity>().dx = 10.f;
  EntityHelper::merge_entity_arrays();

  SystemManager systems;
  systems.register_update_system(std::make_unique<Move>());
  for (int frame = 0; frame < 60; frame++)
    systems.run(1.f / 60.f);

  const float x = ball.get<Position>().x;
  std::printf("after 1s the ball is at x=%.2f\n", x);
  return x > 9.9f && x < 10.1f ? 0 : 1;
}
