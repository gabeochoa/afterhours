#include <cstdio>
#include <string>
#include <tuple>

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

struct PairA : BaseComponent {
  int value = 0;
};
struct PairB : BaseComponent {
  int value = 0;
};
struct PairC : BaseComponent {
  int value = 0;
};
struct NeverRegistered : BaseComponent {};

int main() {
  printf("Running get_unique tests...\n\n");

  {
    EntityCollection coll;
    Entity &host = coll.createEntity();
    host.addComponent<PairA>();
    host.addComponent<PairB>();
    coll.registerSingleton<PairA>(host);
    coll.registerSingleton<PairB>(host);

    auto [a, b] = coll.get_unique<PairA, PairB>();
    check(a != nullptr && b != nullptr, "pair fetch returns both singletons");
    check(a == coll.get_singleton_cmp<PairA>(),
          "pair A is what single get returns");
    check(b == coll.get_singleton_cmp<PairB>(),
          "pair B is what single get returns");
    a->value = 7;
    b->value = 9;
    check(host.get<PairA>().value == 7 && host.get<PairB>().value == 9,
          "writes through the pair land on the components");
    check(std::get<0>(coll.get_unique<PairA, PairB>()) == a,
          "tuple access matches structured binding");
  }

  {
    EntityCollection coll;
    Entity &ea = coll.createEntity();
    ea.addComponent<PairA>();
    Entity &eb = coll.createEntity();
    eb.addComponent<PairB>();
    coll.registerSingleton<PairA>(ea);
    coll.registerSingleton<PairB>(eb);

    auto [a, b] = coll.get_unique<PairA, PairB>();
    check(a == &ea.get<PairA>() && b == &eb.get<PairB>(),
          "pair fetch does not require one shared entity");
  }

  {
    EntityCollection coll;
    Entity &host = coll.createEntity();
    host.addComponent<PairA>();
    coll.registerSingleton<PairA>(host);

    auto [a, b] = coll.get_unique<PairA, PairB>();
    check(a != nullptr && b == nullptr,
          "missing second is nullptr, first still fetched");
    check(coll.get_singleton_cmp<PairB>() == nullptr,
          "single get of the missing one is also nullptr");

    auto [c, d] = coll.get_unique<PairB, PairA>();
    check(c == nullptr && d != nullptr,
          "missing first is nullptr, second still fetched");

    auto [e, f] = coll.get_unique<PairB, NeverRegistered>();
    check(e == nullptr && f == nullptr, "both missing gives two nullptrs");
  }

  {
    EntityCollection coll;
    Entity &host = coll.createEntity();
    host.addComponent<PairA>();
    coll.registerSingleton<PairA>(host);

    auto [a, b] = coll.get_unique<PairA, PairA>();
    check(a != nullptr && a == b, "get_unique<A, A> reads one singleton twice");
  }

  {
    EntityCollection coll;
    Entity &host = coll.createEntity();
    host.addComponent<PairA>();
    host.addComponent<PairB>();
    host.addComponent<PairC>();
    coll.registerSingleton<PairA>(host);
    coll.registerSingleton<PairB>(host);
    coll.registerSingleton<PairC>(host);

    auto [a, b, c] = coll.get_unique<PairA, PairB, PairC>();
    check(a != nullptr && b != nullptr && c != nullptr,
          "triple fetch returns all three");
  }

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
