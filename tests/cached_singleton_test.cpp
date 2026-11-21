// CachedSingleton: a singleton handle resolved once, revalidated by
// generation on each get(), so repeated fetches avoid the singletonMap
// lookup that get_singleton_cmp pays per call.

#define FMT_HEADER_ONLY
#include <fmt/format.h>

#include <afterhours/src/core/cached_singleton.h>

#include <cstdio>

using namespace afterhours;

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name)                                                             \
  static void test_##name();                                                   \
  struct Register_##name {                                                     \
    Register_##name() { register_test(#name, test_##name); }                   \
  } register_##name##_instance;                                                \
  static void test_##name()

struct TestEntry {
  const char *name;
  void (*fn)();
};

static std::vector<TestEntry> &test_registry() {
  static std::vector<TestEntry> r;
  return r;
}

static void register_test(const char *name, void (*fn)()) {
  test_registry().push_back({name, fn});
}

static void check(bool cond, const char *expr, const char *file_, int line) {
  tests_run++;
  if (cond) {
    tests_passed++;
  } else {
    fprintf(stderr, "  FAIL: %s  (%s:%d)\n", expr, file_, line);
  }
}

#define CHECK(...) check((__VA_ARGS__), #__VA_ARGS__, __FILE__, __LINE__)

struct Settings : BaseComponent {
  int volume = 0;
};

struct Fixture {
  EntityCollection collection;

  Fixture() { EntityHelper::set_default_collection(&collection); }
  ~Fixture() { EntityHelper::set_default_collection(nullptr); }

  Entity &add_singleton() {
    Entity &e = collection.createEntity();
    e.addComponent<Settings>();
    collection.registerSingleton<Settings>(e);
    collection.merge_entity_arrays();
    return e;
  }

  void remove_singleton(Entity &e) {
    collection.markIDForCleanup(e.id);
    collection.cleanup();
  }
};

TEST(same_component_across_frames) {
  Fixture f;
  Entity &host = f.add_singleton();
  CachedSingleton<Settings> cache;

  Settings *first = cache.get();
  Settings *second = cache.get();
  CHECK(first != nullptr);
  CHECK(first == second);
  CHECK(cache.valid());
  CHECK(first == &host.get<Settings>());

  first->volume = 42;
  CHECK(cache.get()->volume == 42);
}

TEST(replaced_singleton_refreshes) {
  Fixture f;
  Entity &old_host = f.add_singleton();
  CachedSingleton<Settings> cache;
  Settings *cached = cache.get();
  const EntityHandle old_handle = cache.handle;
  cached->volume = 42;
  CHECK(cached == &old_host.get<Settings>());

  f.remove_singleton(old_host);
  Entity &new_host = f.add_singleton();

  Settings *refetched = cache.get();
  CHECK(refetched == &new_host.get<Settings>());
  CHECK(refetched->volume == 0);
  CHECK(cache.handle != old_handle);
  OptEntity resolved = EntityHelper::resolve(cache.handle);
  CHECK(resolved.has_value());
}

TEST(removed_singleton_returns_nullptr) {
  Fixture f;
  Entity &host = f.add_singleton();
  CachedSingleton<Settings> cache;
  Settings *cached = cache.get();
  CHECK(cached != nullptr);

  f.remove_singleton(host);
  CHECK(cache.get() == nullptr);
  CHECK(!cache.valid());
}

int main() {
  printf("Running CachedSingleton tests...\n\n");
  for (const auto &entry : test_registry()) {
    printf("  %s\n", entry.name);
    entry.fn();
  }
  printf("\n%d/%d tests passed.\n", tests_passed, tests_run);
  return tests_passed == tests_run ? 0 : 1;
}
