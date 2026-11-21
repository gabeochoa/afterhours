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

static EntityCollection &singleton_default() {
  return EntityHelper::get().default_collection;
}

static bool bound_to(EntityCollection &c) {
  return &EntityHelper::get_default_collection() == &c;
}

static int early_return(EntityCollection &c) {
  EntityHelper::ScopedCollection scope(&c);
  CHECK(bound_to(c));
  return 42;
}

int main() {
  CHECK(bound_to(singleton_default()));

  EntityCollection a, b;
  {
    EntityHelper::ScopedCollection outer(&a);
    CHECK(bound_to(a));
    {
      EntityHelper::ScopedCollection inner(&b);
      CHECK(bound_to(b));
    }
    CHECK(bound_to(a));
  }
  CHECK(bound_to(singleton_default()));

  EntityHelper::set_default_collection(&a);
  {
    EntityHelper::ScopedCollection scope(nullptr);
    CHECK(bound_to(singleton_default()));
  }
  CHECK(bound_to(a));
  EntityHelper::set_default_collection(nullptr);
  CHECK(bound_to(singleton_default()));

  try {
    EntityHelper::ScopedCollection scope(&a);
    CHECK(bound_to(a));
    throw 1;
  } catch (int) {
  }
  CHECK(bound_to(singleton_default()));

  CHECK(early_return(b) == 42);
  CHECK(bound_to(singleton_default()));

  std::printf("%d/%d checks passed\n", checks - failures, checks);
  if (failures == 0)
    std::printf("All checks passed!\n");
  return failures == 0 ? 0 : 1;
}
