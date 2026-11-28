#include <cstdio>

#include "../../../example_helper.h"

using namespace afterhours;
using namespace afterhours::examples;

int steps = 0;

int main() {
  Checks checks;
  SystemManager systems;
  systems.fixed_tick_rate = 1.f / 60.f;
  systems.max_fixed_ticks = 5;
  systems.register_fixed_update_system([](float dt) {
    steps++;
    (void)dt;
  });

  systems.run(1.f / 30.f);
  printf("a 33ms frame ran %d physics steps of %.4fs\n", steps,
         systems.fixed_tick_rate);
  const int normal = steps;

  steps = 0;
  systems.run(2.f);
  printf("a 2s hitch ran %d steps, not 120\n", steps);

  checks.check(normal == 2 && steps == 5,
               "fixed ticks run twice for 33ms and cap at 5 on a hitch");
  return checks.finish("fixed_tick_rate");
}
