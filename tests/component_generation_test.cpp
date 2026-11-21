// Per-type membership generation: add/remove bumps only that type.
#define FMT_HEADER_ONLY
#include <fmt/format.h>

#include <afterhours/ah.h>

#include <cstdio>

using namespace afterhours;

static int checks = 0;
static int failures = 0;
static void check(bool cond, const char *expr, int line) {
  checks++;
  if (!cond) {
    failures++;
    std::fprintf(stderr, "  FAIL: %s  (line %d)\n", expr, line);
  }
}
#define CHECK(...) check((__VA_ARGS__), #__VA_ARGS__, __LINE__)

struct Pos : BaseComponent { int x = 0; };
struct Vel : BaseComponent { int x = 0; };

int main() {
  Entity e;
  const auto p0 = component_generation<Pos>();
  const auto v0 = component_generation<Vel>();

  e.addComponent<Pos>();
  CHECK(component_generation<Pos>() == p0 + 1);
  CHECK(component_generation<Vel>() == v0);

  // Field mutation is deliberately not tracked.
  e.get<Pos>().x = 42;
  CHECK(component_generation<Pos>() == p0 + 1);

  e.addComponentIfMissing<Pos>();
  CHECK(component_generation<Pos>() == p0 + 1);

  e.removeComponent<Pos>();
  CHECK(component_generation<Pos>() == p0 + 2);
  CHECK(component_generation<Vel>() == v0);
  e.removeComponentIfExists<Pos>();
  CHECK(component_generation<Pos>() == p0 + 2);

  // Entity leaving a collection bumps its attached types only.
  EntityCollection ec;
  Entity &a = ec.createEntity();
  a.addComponent<Vel>();
  ec.merge_entity_arrays();
  const auto p1 = component_generation<Pos>();
  const auto v1 = component_generation<Vel>();
  ec.markIDForCleanup(a.id);
  ec.cleanup();
  CHECK(component_generation<Vel>() == v1 + 1);
  CHECK(component_generation<Pos>() == p1);

  // The ComponentID overload agrees with the typed one.
  CHECK(component_generation(components::get_type_id<Vel>()) ==
        component_generation<Vel>());

  std::printf("component_generation_test: %d checks, %d failures\n", checks,
              failures);
  return failures == 0 ? 0 : 1;
}
