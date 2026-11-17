#define SKIP_ENTITY_QUERY_MODIFICATIONS
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
#define CHECK(expr) check((expr), #expr, __LINE__)

struct Tag : BaseComponent {};

int main() {
  EntityCollection ec;
  for (int i = 0; i < 10; ++i) {
    Entity &e = ec.createEntity();
    if (i % 2 == 0)
      e.addComponent<Tag>();
  }
  ec.cleanup();
  const EntityID tagged = ec.get_entities()[4]->id;
  const EntityID untagged = ec.get_entities()[5]->id;

  auto got = EntityQuery(ec).whereID(tagged).gen();
  CHECK(got.size() == 1 && got[0].get().id == tagged);
  CHECK(EntityQuery(ec).whereID(999999).gen().empty());
  CHECK(EntityQuery(ec).whereID(untagged).whereHasComponent<Tag>().gen().empty());
  CHECK(EntityQuery(ec).whereHasComponent<Tag>().gen().size() == 5);
  CHECK(EntityQuery(ec).whereHasComponent<Tag>().take(2).gen().size() == 2);

  std::printf("query_skip_mods_test: %d/%d checks passed\n", checks - failures,
              checks);
  return failures == 0 ? 0 : 1;
}
