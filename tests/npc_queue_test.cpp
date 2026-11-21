
#include <afterhours/src/plugins/npc_queue.h>

#include <cmath>
#include <cstdio>
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
  if (cond) {
    tests_passed++;
  } else {
    fprintf(stderr, "  FAIL: %s  (%s:%d)\n", expr, file, line);
  }
}

#define CHECK(expr) check((expr), #expr, __FILE__, __LINE__)

static bool near(float a, float b) { return std::fabs(a - b) < 1e-4f; }

TEST(join_order) {
  NpcQueue<int> q;
  CHECK(q.empty());
  CHECK(q.join(10));
  CHECK(q.join(20));
  CHECK(q.join(30));
  CHECK(q.size() == 3);
  CHECK(q.front().has_value() && *q.front() == 10);
  CHECK(q.position_of(10) == std::optional<size_t>{0});
  CHECK(q.position_of(20) == std::optional<size_t>{1});
  CHECK(q.position_of(30) == std::optional<size_t>{2});
  CHECK(q.members() == std::vector<int>({10, 20, 30}));
}

TEST(duplicate_join_is_noop) {
  NpcQueue<int> q;
  CHECK(q.join(7));
  CHECK(q.join(8));
  CHECK(!q.join(7));
  CHECK(!q.join_at(8, 0));
  CHECK(q.size() == 2);
  CHECK(q.members() == std::vector<int>({7, 8}));
}

TEST(join_at_inserts_at_slot) {
  NpcQueue<int> q;
  CHECK(q.join(1));
  CHECK(q.join(3));
  CHECK(q.join_at(2, 1));
  CHECK(q.members() == std::vector<int>({1, 2, 3}));
  CHECK(q.join_at(9, 99));
  CHECK(q.members() == std::vector<int>({1, 2, 3, 9}));
}

TEST(mid_queue_removal_compacts) {
  NpcQueue<int> q;
  for (int id : {1, 2, 3, 4, 5})
    CHECK(q.join(id));
  CHECK(q.leave(3));
  CHECK(q.members() == std::vector<int>({1, 2, 4, 5}));
  CHECK(q.position_of(4) == std::optional<size_t>{2});
  CHECK(q.position_of(5) == std::optional<size_t>{3});
  CHECK(!q.leave(3));
  CHECK(!q.leave(99));
  CHECK(q.size() == 4);
}

TEST(at_front_and_front) {
  NpcQueue<int> q;
  CHECK(!q.front().has_value());
  CHECK(!q.at_front(1));
  CHECK(q.join(1));
  CHECK(q.join(2));
  CHECK(q.at_front(1));
  CHECK(!q.at_front(2));
  CHECK(!q.at_front(99));
}

TEST(advance_pops_front_and_notifies_next) {
  NpcQueue<int> q;
  std::vector<int> front_calls;
  q.set_at_front_callback([&](int id) { front_calls.push_back(id); });
  CHECK(q.join(1));
  CHECK(q.join(2));
  CHECK(q.join(3));
  CHECK(front_calls == std::vector<int>({1}));
  const auto popped = q.advance();
  CHECK(popped.has_value() && *popped == 1);
  CHECK(front_calls == std::vector<int>({1, 2}));
  CHECK(q.at_front(2));
  CHECK(q.advance() == std::optional<int>{2});
  CHECK(front_calls == std::vector<int>({1, 2, 3}));
  CHECK(q.advance() == std::optional<int>{3});
  CHECK(front_calls == std::vector<int>({1, 2, 3}));
  CHECK(!q.advance().has_value());
  CHECK(q.empty());
}

TEST(at_front_callback_fires_only_on_new_front) {
  NpcQueue<int> q;
  std::vector<int> front_calls;
  q.set_at_front_callback([&](int id) { front_calls.push_back(id); });
  CHECK(q.join_at(5, 0));
  CHECK(front_calls == std::vector<int>({5}));
  CHECK(q.join_at(4, 0));
  CHECK(front_calls == std::vector<int>({5, 4}));
  CHECK(q.join(6));
  CHECK(front_calls == std::vector<int>({5, 4}));
  CHECK(q.leave(5));
  CHECK(front_calls == std::vector<int>({5, 4}));
  CHECK(q.leave(4));
  CHECK(front_calls == std::vector<int>({5, 4, 6}));
  CHECK(q.at_front(6));
  q.clear();
  CHECK(front_calls == std::vector<int>({5, 4, 6}));
  CHECK(q.empty());
}

TEST(slot_position_along_line) {
  const std::vector<Vector2Type> line{{0.f, 0.f}, {40.f, 0.f}, {40.f, 30.f}};
  const Vector2Type p0 = npc_queue::slot_position(line, 0, 20.f);
  CHECK(near(p0.x, 0.f) && near(p0.y, 0.f));
  const Vector2Type p1 = npc_queue::slot_position(line, 1, 20.f);
  CHECK(near(p1.x, 20.f) && near(p1.y, 0.f));
  const Vector2Type p2 = npc_queue::slot_position(line, 2, 20.f);
  CHECK(near(p2.x, 40.f) && near(p2.y, 0.f));
  const Vector2Type p3 = npc_queue::slot_position(line, 3, 20.f);
  CHECK(near(p3.x, 40.f) && near(p3.y, 20.f));
  const Vector2Type p4 = npc_queue::slot_position(line, 9, 20.f);
  CHECK(near(p4.x, 40.f) && near(p4.y, 30.f));
  const std::vector<Vector2Type> none;
  const Vector2Type p5 = npc_queue::slot_position(none, 0, 20.f);
  CHECK(near(p5.x, 0.f) && near(p5.y, 0.f));
}


int main() {
  printf("Running npc_queue tests...\n\n");

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
