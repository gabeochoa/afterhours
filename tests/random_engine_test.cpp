// random_engine_test.cpp
// Coverage for the seedable RandomEngine (src/random_engine.h) and the
// ECS-integrated random path (EntityQuery::gen_random). The whole point of a
// seedable RNG is reproducibility: same seed -> same sequence. This locks that
// contract, plus the range/index bounds and the string-seed hashing.
//
// Build (from tests/, via the Makefile):  make random_engine_test
// Standalone (note the -DAFTER_HOURS_ENABLE_RANDOM build flag — random.h's own
// ODR guidance prefers the flag over an in-source define):
//   clang++ -std=c++20 -DAFTER_HOURS_ENABLE_RANDOM \
//     -isystem <root-with-afterhours-symlink> -isystem vendor \
//     random_engine_test.cpp -o /tmp/t && /tmp/t

#define AFTER_HOURS_ENTITY_HELPER
#define AFTER_HOURS_ENTITY_QUERY
#define AFTER_HOURS_SYSTEM
#include <afterhours/ah.h>
#include <afterhours/src/random_engine.h>

#include <cstdio>
#include <cstring>
#include <vector>

using namespace afterhours;

static int tests_run = 0, tests_passed = 0;
static void check(bool cond, const char *expr, const char *file, int line) {
  tests_run++;
  if (cond) tests_passed++;
  else fprintf(stderr, "  FAIL: %s  (%s:%d)\n", expr, file, line);
}
#define CHECK(expr) check((expr), #expr, __FILE__, __LINE__)

// Same seed -> identical sequence (the core reproducibility contract).
void test_same_seed_same_sequence() {
  auto &r = RandomEngine::get();
  r.set_seed(std::uint32_t{12345});
  std::vector<int> a;
  for (int i = 0; i < 16; i++) a.push_back(r.get_int(0, 1000000));
  r.set_seed(std::uint32_t{12345});
  std::vector<int> b;
  for (int i = 0; i < 16; i++) b.push_back(r.get_int(0, 1000000));
  CHECK(a == b);
}

// Different seeds -> (almost certainly) different sequences.
void test_different_seed_differs() {
  auto &r = RandomEngine::get();
  r.set_seed(std::uint32_t{1});
  std::vector<int> a;
  for (int i = 0; i < 16; i++) a.push_back(r.get_int(0, 1000000));
  r.set_seed(std::uint32_t{2});
  std::vector<int> b;
  for (int i = 0; i < 16; i++) b.push_back(r.get_int(0, 1000000));
  CHECK(a != b);
}

// String seed is stable: same string -> same sequence, and the seed is the
// specified FNV-1a 32-bit hash of the bytes (not std::hash, which is
// implementation-defined and differed across STLs).
void test_string_seed_stable() {
  auto &r = RandomEngine::get();
  r.set_seed(std::string{"level-3"});
  int x = r.get_int(0, 1000000);
  r.set_seed(std::string{"level-3"});
  int y = r.get_int(0, 1000000);
  CHECK(x == y);
  // FNV-1a 32 over "level-3" — pinned literal, STL-independent.
  CHECK(r.get_seed() == 2911412057u);
  CHECK(x == 288888);
}

// Golden vectors for the bit-stable contract (G-P12). Every literal is the
// exact output of the specified algorithms (mt19937 raw draws, FNV-1a seed,
// Lemire int mapping, 24-bit float mapping) for the given seed. If any
// hash/mapping/engine behavior drifts — including a future std::hash or
// std::uniform_* swap — these fail loudly on every STL.
void test_golden_vectors() {
  // Raw draws: mt19937, seed 12345.
  RngStream raw(std::uint32_t{12345});
  const std::uint32_t expected_raw[] = {
      3992670690u, 3823185381u, 1358822685u, 561383553u,
      789925284u,  170765737u,  878579710u,  3549516158u};
  for (auto want : expected_raw) CHECK(raw.next_u32() == want);

  // Raw draws cross-check with a second seed.
  RngStream raw42(std::uint32_t{42});
  CHECK(raw42.next_u32() == 1608637542u);
  CHECK(raw42.next_u32() == 3421126067u);
  CHECK(raw42.next_u32() == 4083286876u);
  CHECK(raw42.next_u32() == 787846414u);

  // Mapped ints: Lemire [0, 1000000], seed 12345.
  RngStream ints(std::uint32_t{12345});
  const int expected_ints[] = {929617, 890155, 316375, 130707,
                               183918, 39759,  204560, 826436};
  for (int want : expected_ints) CHECK(ints.get_int(0, 1000000) == want);

  // Mapped floats: 24-bit construction [-1, 1), seed 12345 — compare bits.
  RngStream floats(std::uint32_t{12345});
  const std::uint32_t expected_float_bits[] = {0x3f5bf6a2u, 0x3f47c25au,
                                               0xbebc080cu, 0xbf3d13f0u};
  for (auto want : expected_float_bits) {
    float got = floats.get_float(-1.0f, 1.0f);
    std::uint32_t got_bits = 0;
    memcpy(&got_bits, &got, sizeof(got_bits));
    CHECK(got_bits == want);
    CHECK(got >= -1.0f && got < 1.0f);
  }

  // Bools derive from the int mapping (get_int(0,1) == 1), seed 12345.
  RngStream bools(std::uint32_t{12345});
  const int expected_bools[] = {1, 1, 0, 0, 0, 0, 0, 1};
  for (int want : expected_bools) CHECK((bools.get_bool() ? 1 : 0) == want);

  // String-seeded stream: seed value and first mapped draw are pinned.
  RngStream named(std::string{"level-3"});
  CHECK(named.get_seed() == 2911412057u);
  CHECK(named.get_int(0, 1000000) == 288888);

  // Degenerate and full ranges stay exact.
  RngStream degenerate(std::uint32_t{7});
  CHECK(degenerate.get_int(42, 42) == 42);
  RngStream full(std::uint32_t{7});
  CHECK(full.get_int(INT32_MIN, INT32_MAX) == -1819742033);
  CHECK(full.get_int(INT32_MIN, INT32_MAX) == -1171069756);
  RngStream flat(std::uint32_t{7});
  CHECK(flat.get_float(3.0f, 3.0f) == 3.0f);
}

// get_int is inclusive [a,b] and stays in range.
void test_int_range_bounds() {
  auto &r = RandomEngine::get();
  r.set_seed(std::uint32_t{7});
  bool in_range = true;
  for (int i = 0; i < 1000; i++) {
    int v = r.get_int(5, 10);
    if (v < 5 || v > 10) { in_range = false; break; }
  }
  CHECK(in_range);
  // degenerate range [k,k] always yields k
  CHECK(r.get_int(42, 42) == 42);
}

// get_float stays within [a,b).
void test_float_range_bounds() {
  auto &r = RandomEngine::get();
  r.set_seed(std::uint32_t{9});
  bool in_range = true;
  for (int i = 0; i < 1000; i++) {
    float v = r.get_float(-1.0f, 1.0f);
    if (v < -1.0f || v > 1.0f) { in_range = false; break; }
  }
  CHECK(in_range);
}

// get_index: valid index for non-empty, -1 for empty.
void test_get_index() {
  auto &r = RandomEngine::get();
  r.set_seed(std::uint32_t{3});
  std::vector<int> empty;
  CHECK(r.get_index(empty) == -1);
  std::vector<int> five(5, 0);
  bool valid = true;
  for (int i = 0; i < 1000; i++) {
    int idx = r.get_index(five);
    if (idx < 0 || idx >= 5) { valid = false; break; }
  }
  CHECK(valid);
}

// ECS-integrated gen_random: seeded -> reproducible entity pick; empty -> none.
void test_gen_random_reproducible() {
  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();
  struct Mark : BaseComponent {};
  for (int i = 0; i < 10; i++)
    EntityHelper::createEntity().addComponent<Mark>();
  EntityHelper::merge_entity_arrays();

  RandomEngine::get().set_seed(std::uint32_t{555});
  auto first = EntityQuery().whereHasComponent<Mark>().gen_random();
  RandomEngine::get().set_seed(std::uint32_t{555});
  auto second = EntityQuery().whereHasComponent<Mark>().gen_random();
  CHECK(first.has_value() && second.has_value());
  CHECK(first.has_value() && second.has_value() &&
        first->id == second->id); // same seed -> same pick

  // empty query -> no value
  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();
  struct Absent : BaseComponent {};
  auto none = EntityQuery().whereHasComponent<Absent>().gen_random();
  CHECK(!none.has_value());
}

void test_stream_save_restores_mid_sequence() {
  RngStream live(std::uint32_t{2024});
  for (int i = 0; i < 37; i++) (void)live.get_int(0, 1000);
  const std::string saved = live.save();
  std::vector<int> expected;
  for (int i = 0; i < 20; i++) expected.push_back(live.get_int(0, 1000000));

  auto restored = RngStream::load(saved);
  CHECK(restored.has_value());
  if (!restored) return;
  CHECK(restored->get_seed() == 2024u);
  std::vector<int> got;
  for (int i = 0; i < 20; i++) got.push_back(restored->get_int(0, 1000000));
  CHECK(got == expected);
  CHECK(!RngStream::load("not a stream").has_value());
}

void test_streams_are_independent() {
  RngStream shop(std::uint32_t{7}), battle(std::uint32_t{7});
  RngStream battle_alone(std::uint32_t{7});
  for (int i = 0; i < 50; i++) (void)shop.get_int(0, 100);
  bool same = true;
  for (int i = 0; i < 20; i++)
    same = same && battle.get_int(0, 1000) == battle_alone.get_int(0, 1000);
  CHECK(same);
}

void test_gen_random_takes_a_stream() {
  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();
  struct Pick : BaseComponent {};
  for (int i = 0; i < 10; i++)
    EntityHelper::createEntity().addComponent<Pick>();
  EntityHelper::merge_entity_arrays();
  RngStream a(std::uint32_t{99}), b(std::uint32_t{99});
  auto first = EntityQuery().whereHasComponent<Pick>().gen_random(a);
  auto second = EntityQuery().whereHasComponent<Pick>().gen_random(b);
  CHECK(first.has_value() && second.has_value() && first->id == second->id);
  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();
}

int main() {
  printf("=== RandomEngine tests ===\n\n");
  struct T { const char *n; void (*f)(); };
  T tests[] = {
    {"same_seed_same_sequence", test_same_seed_same_sequence},
    {"different_seed_differs", test_different_seed_differs},
    {"string_seed_stable", test_string_seed_stable},
    {"golden_vectors", test_golden_vectors},
    {"int_range_bounds", test_int_range_bounds},
    {"float_range_bounds", test_float_range_bounds},
    {"get_index", test_get_index},
    {"gen_random_reproducible", test_gen_random_reproducible},
    {"stream_save_restores_mid_sequence", test_stream_save_restores_mid_sequence},
    {"streams_are_independent", test_streams_are_independent},
    {"gen_random_takes_a_stream", test_gen_random_takes_a_stream},
  };
  for (auto &t : tests) { printf("  Running: %s\n", t.n); t.f(); }
  printf("\n%d/%d checks passed\n", tests_passed, tests_run);
  if (tests_passed != tests_run) { printf("FAILURES: %d\n", tests_run - tests_passed); return 1; }
  printf("All checks passed!\n");
  return 0;
}
