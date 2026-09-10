// singleton_iteration_test.cpp
// A system whose components are all registered singletons cannot match more
// than one entity, so it should not be walked past every other one. puzzle
// profiled that scan at 37% of a frame on a screen with no visible UI.
//
// The shortcut has to reach exactly the same entities as the scan did, so
// these check what for_each saw, not how long it took.

#include <cstdio>
#include <string>
#include <vector>

#include <afterhours/src/core/system.h>

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

struct Lonely : BaseComponent {};
struct AlsoLonely : BaseComponent {};
struct Common : BaseComponent {};

// Records which entities for_each actually reached.
template <typename... Cs> struct Recorder : System<Cs...> {
  std::vector<EntityID> seen;
  void for_each_with(Entity &e, Cs &..., const float) override {
    seen.push_back(e.id);
  }
};

int main() {
  printf("Running singleton iteration tests...\n\n");

  EntityCollection coll;
  auto &entities = coll.get_entities_for_mod();

  // One entity holds the singletons; the rest are the crowd to skip.
  Entity &host = coll.createEntity();
  host.addComponent<Lonely>();
  host.addComponent<AlsoLonely>();
  host.addComponent<Common>();
  coll.registerSingleton<Lonely>(host);
  coll.registerSingleton<AlsoLonely>(host);

  for (int i = 0; i < 50; i++) {
    coll.createEntity().addComponent<Common>();
  }
  coll.merge_entity_arrays();

  // --- the shortcut fires and finds the right entity ----------------------
  // Asked directly, because reaching one entity proves nothing on its own --
  // the full scan reaches one too, since only the host has Lonely.
  {
    Recorder<Lonely> sys;
    OptEntity found = sys.only_possible_match(coll);
    check(found.has_value() && found.value() == &host,
          "a singleton component resolves to its entity without scanning");

    Recorder<Common> scanner;
    check(!scanner.only_possible_match(coll).has_value(),
          "a plain component does not, so the scan still runs");

    run_system_over(sys, entities, 0.f, coll, false);
    check(sys.seen.size() == 1, "one singleton component reaches one entity");
    check(!sys.seen.empty() && sys.seen[0] == host.id, "and it is the host");
  }

  // --- two singletons on the same entity still resolve to it --------------
  {
    Recorder<Lonely, AlsoLonely> sys;
    run_system_over(sys, entities, 0.f, coll, false);
    check(sys.seen.size() == 1, "two singletons on one entity reach it once");
  }

  // --- a non-singleton component falls back to the full scan --------------
  {
    Recorder<Common> sys;
    run_system_over(sys, entities, 0.f, coll, false);
    check(sys.seen.size() == 51,
          "a plain component still visits every holder");
  }

  // --- mixed: one singleton, one not. The singleton narrows it -----------
  {
    Recorder<Lonely, Common> sys;
    run_system_over(sys, entities, 0.f, coll, false);
    check(sys.seen.size() == 1,
          "a singleton alongside a plain component still narrows to one");
  }

  // --- two singletons on different entities match nothing -----------------
  {
    EntityCollection split;
    auto &split_entities = split.get_entities_for_mod();
    Entity &a = split.createEntity();
    a.addComponent<Lonely>();
    Entity &b = split.createEntity();
    b.addComponent<AlsoLonely>();
    split.registerSingleton<Lonely>(a);
    split.registerSingleton<AlsoLonely>(b);
    split.merge_entity_arrays();

    Recorder<Lonely, AlsoLonely> sys;
    run_system_over(sys, split_entities, 0.f, split, false);
    check(sys.seen.empty(),
          "singletons on different entities match nothing, not one of them");
  }

  // --- include_derived_children opts out ----------------------------------
  // The derived component is not the one in the singleton map, so the entity
  // holding it is not the one the shortcut would find.
  {
    Recorder<Lonely> sys;
    sys.include_derived_children = true;
    check(!sys.only_possible_match(coll).has_value(),
          "derived children fall back to the scan");
  }

  // --- should_iterate() is still honoured ---------------------------------
  {
    struct NeverIterates : Recorder<Lonely> {
      bool should_iterate() const override { return false; }
    };
    NeverIterates sys;
    run_system_over(sys, entities, 0.f, coll, false);
    check(sys.seen.empty(), "should_iterate false skips the shortcut too");
  }

  // --- a retired singleton is skipped when the caller asks ----------------
  // The UI runner passes skip_cleanup because a widget retired this frame is
  // still in the list.
  {
    host.cleanup = true;
    Recorder<Lonely> skipping;
    run_system_over(skipping, entities, 0.f, coll, true);
    check(skipping.seen.empty(), "skip_cleanup applies to the shortcut");

    Recorder<Lonely> not_skipping;
    run_system_over(not_skipping, entities, 0.f, coll, false);
    check(not_skipping.seen.size() == 1, "and does not when not asked");
    host.cleanup = false;
  }

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
