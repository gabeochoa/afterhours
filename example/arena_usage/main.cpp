#include <cassert>
#include <cstdio>

#include "../../src/memory/arena.h"

int main(int, char **) {
  using namespace afterhours;

  Arena arena(256);

  auto *value = arena.create<int>(123);
  assert(value && *value == 123);

  ArenaVector<int> values(arena, 2);
  values.push_back(1);
  values.push_back(2);
  values.push_back(3);
  assert(values.size() == 3);

  ArenaEntityMap<int> map(arena, 2);
  map.get_or_create(1) = 42;
  assert(map.get(1) && *map.get(1) == 42);

  std::printf("arena_usage: ok\n");
  return 0;
}

