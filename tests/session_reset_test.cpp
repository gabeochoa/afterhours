#define FMT_HEADER_ONLY
#include <fmt/format.h>

#include <afterhours/ah.h>

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

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

static void check(bool cond, const char *expr, const char *file, int line) {
  tests_run++;
  if (cond)
    tests_passed++;
  else
    fprintf(stderr, "  FAIL: %s  (%s:%d)\n", expr, file, line);
}
#define CHECK(expr) check((expr), #expr, __FILE__, __LINE__)

struct Pos : BaseComponent {
  float x = 0, y = 0;
  Pos() = default;
  Pos(float x_, float y_) : x(x_), y(y_) {}
};
struct Health : BaseComponent {
  int hp = 0;
  Health() = default;
  explicit Health(int h) : hp(h) {}
};
struct Session : BaseComponent {
  int score = 0;
  explicit Session(int s) : score(s) {}
};

struct Snapshot {
  std::string text;
  std::uint64_t checksum = 1469598103934665603ull;
  void add(const std::string &s) {
    text += s;
    for (unsigned char c : s) {
      checksum ^= c;
      checksum *= 1099511628211ull;
    }
  }
};

static Snapshot build_world(EntityCollection &ec) {
  Entity &perm = ec.createPermanentEntity();
  perm.addComponent<Session>(7);
  ec.registerSingleton<Session>(perm);
  for (int i = 0; i < 3; ++i) {
    Entity &e = ec.createEntity();
    e.addComponent<Pos>(static_cast<float>(i), static_cast<float>(i * 2));
    e.addComponent<Health>(100 - i);
  }
  ec.merge_entity_arrays();

  Snapshot snap;
  snap.add("count=" + std::to_string(ec.get_entities().size()) + ";");
  for (const auto &sp : ec.get_entities()) {
    EntityHandle h = ec.handle_for(*sp);
    snap.add("id=" + std::to_string(sp->id) +
             ",slot=" + std::to_string(h.slot) +
             ",gen=" + std::to_string(h.gen));
    if (sp->has<Pos>())
      snap.add(",pos=" + std::to_string(sp->get<Pos>().x) + "/" +
               std::to_string(sp->get<Pos>().y));
    if (sp->has<Health>())
      snap.add(",hp=" + std::to_string(sp->get<Health>().hp));
    if (sp->has<Session>())
      snap.add(",score=" + std::to_string(sp->get<Session>().score));
    snap.add(";");
  }
  return snap;
}

TEST(reset_rebuild_is_identical) {
  EntityCollection ec;
  ec.add_index<Health>([](const Health &h) { return h.hp; });
  ec.reset_for_new_session();
  const std::uint64_t gen0 = ec.session_generation();
  Snapshot first = build_world(ec);
  CHECK(ec.get_entities().size() == 4);
  CHECK(ec.get_entities()[0]->id == 0);
  CHECK(ec.has_singleton<Session>());
  CHECK((ec.indexed<Health, int>(100).size() == 1));

  ec.get_entities()[1]->cleanup = true;
  ec.cleanup();
  ec.createEntity().addComponent<Health>(1);
  ec.get_singleton_cmp<Session>()->score = 999;

  ec.reset_for_new_session();
  CHECK(ec.session_generation() == gen0 + 1);
  CHECK(ec.get_entities().empty());
  CHECK(ec.get_temp().empty());
  CHECK(ec.permanent_ids.empty());
  CHECK(!ec.has_singleton<Session>());
  CHECK(ec.pool_size() == 0);
  CHECK(ec.free_ids_.empty());
  CHECK((ec.indexed<Health, int>(100).empty()));

  Snapshot second = build_world(ec);
  CHECK(second.text == first.text);
  CHECK(second.checksum == first.checksum);
  CHECK((ec.indexed<Health, int>(100).size() == 1));
}

TEST(second_reset_is_also_identical) {
  EntityCollection ec;
  ec.reset_for_new_session();
  Snapshot first = build_world(ec);
  ec.reset_for_new_session();
  ec.reset_for_new_session();
  Snapshot second = build_world(ec);
  CHECK(second.checksum == first.checksum);
}

TEST(entity_helper_reset_delegates) {
  EntityHelper::reset_for_new_session();
  const std::uint64_t gen = EntityHelper::session_generation();
  Entity &a = EntityHelper::createEntity();
  const EntityID first_id = a.id;
  EntityHelper::reset_for_new_session();
  CHECK(EntityHelper::session_generation() == gen + 1);
  CHECK(EntityHelper::get_entities().empty());
  Entity &b = EntityHelper::createEntity();
  CHECK(b.id == first_id);
  EntityHelper::reset_for_new_session();
}

int main() {
  printf("Running session reset tests...\n\n");
  for (const auto &entry : test_registry()) {
    printf("  %s\n", entry.name);
    entry.fn();
  }
  printf("\n%d/%d tests passed.\n", tests_passed, tests_run);
  if (tests_passed != tests_run) {
    printf("SOME TESTS FAILED!\n");
    return 1;
  }
  printf("ALL TESTS PASSED.\n");
  return 0;
}
