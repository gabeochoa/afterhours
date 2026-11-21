// add_singleton: one call creates a permanent entity, adds the component,
// registers it as the singleton, and merges it into the entity list.

#include <cstdio>
#include <string>

#include <afterhours/src/core/entity_collection.h>

using namespace afterhours;

static int checks_run = 0;
static int checks_passed = 0;

static void check(bool cond, const std::string &what) {
  checks_run++;
  if (cond) {
    checks_passed++;
  } else {
    fprintf(stderr, "  FAIL: %s\n", what.c_str());
  }
}

struct Config : BaseComponent {
  int value = 0;
  explicit Config(int v = 0) : value(v) {}
};

struct Other : BaseComponent {};

int main() {
  printf("Running add_singleton tests...\n\n");

  EntityCollection coll;

  // One call: the component is retrievable as the singleton, with args
  // forwarded, and the entity is already in the merged list.
  {
    Config &cfg = coll.add_singleton<Config>(42);
    check(cfg.value == 42, "constructor args forwarded");
    check(coll.has_singleton<Config>(), "singleton registered");
    check(coll.get_singleton_cmp<Config>() == &cfg,
          "get_singleton_cmp returns the added component");
    check(coll.get_singleton<Config>().get().has<Config>(),
          "get_singleton entity holds the component");
    bool merged = false;
    for (const auto &e : coll.get_entities_for_mod()) {
      if (e && e->has<Config>())
        merged = true;
    }
    check(merged, "entity merged into the entity list by the one call");
  }

  // A second component type works the same way.
  {
    Other &other = coll.add_singleton<Other>();
    check(coll.get_singleton_cmp<Other>() == &other, "second singleton type");
  }

  // Duplicate call: same as the multi-call sequence. registerSingleton
  // logs and emplace keeps the first registration, so the singleton still
  // points at the first entity and the second component is unregistered.
  {
    Config *first = coll.get_singleton_cmp<Config>();
    Config &second = coll.add_singleton<Config>(7);
    check(second.value == 7, "duplicate call returns its own component");
    check(coll.get_singleton_cmp<Config>() == first,
          "duplicate call does not replace the registered singleton");
    check(coll.has_singleton<Config>(), "still registered after duplicate");
  }

  printf("%d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
