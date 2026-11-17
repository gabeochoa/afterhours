#include <cstdio>
#include <string>

#include "../../../../ah.h"

using namespace afterhours;

struct Health : BaseComponent {
  int hp = 3;
};

struct RegenerateHealth : System<Health> {
  void for_each_with(Entity &, Health &health, float) override { health.hp++; }
};

int main() {
  RegenerateHealth regen;
  const std::string name(regen.name());
  printf("profiler and crash reports call this system \"%s\"\n", name.c_str());
  return name == "RegenerateHealth" ? 0 : 1;
}
