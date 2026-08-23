// typed_entity_handle_test.cpp
//
// TypedEntityHandle and EntityHandle equality, both moved up from a downstream
// game that had been carrying them in a reopened `namespace afterhours`.
//
// The interesting property is staleness: a handle is (slot, gen), so once an
// entity is destroyed and its slot recycled, the old handle must NOT compare
// equal to the new occupant and must NOT resolve to it. That is the whole
// reason handles exist instead of raw pointers or bare ids.
//
// Build (from afterhours/tests/):
//   make typed_entity_handle_test
// Run:
//   ./typed_entity_handle_test

#define FMT_HEADER_ONLY
#include <fmt/format.h>

#include <afterhours/ah.h>

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

struct Marker : BaseComponent {};
struct Other : BaseComponent {};

// ============================================================================
// EntityHandle equality
// ============================================================================

TEST(handle_equality_is_slot_and_gen) {
  constexpr EntityHandle a{3, 1};
  constexpr EntityHandle b{3, 1};
  constexpr EntityHandle same_slot_newer{3, 2};
  constexpr EntityHandle other_slot{4, 1};

  CHECK(a == b);
  CHECK(!(a != b));
  // A recycled slot is a DIFFERENT entity. If this ever compares equal, every
  // stale handle in the program silently aliases whatever took its place.
  CHECK(a != same_slot_newer);
  CHECK(a != other_slot);
}

TEST(handle_equality_is_constexpr) {
  // Not a runtime assertion -- this fails the build if the operators stop
  // being usable in a constant expression.
  static_assert(EntityHandle{1, 1} == EntityHandle{1, 1});
  static_assert(EntityHandle{1, 1} != EntityHandle{1, 2});
  CHECK(true);
}

TEST(invalid_handles_compare_equal_to_each_other) {
  CHECK(EntityHandle::invalid() == EntityHandle::invalid());
  CHECK(!EntityHandle::invalid().is_valid());
}

// ============================================================================
// TypedEntityHandle
// ============================================================================

TEST(default_constructed_is_invalid) {
  TypedEntityHandle<Marker> h;
  CHECK(h.is_invalid());
  CHECK(!h.is_valid());
  CHECK(h == EntityHandle::invalid());
  CHECK(TypedEntityHandle<Marker>::invalid().is_invalid());
}

TEST(constructs_from_entity_and_resolves_back) {
  Entity &e = EntityHelper::createEntity();
  e.addComponent<Marker>();
  EntityHelper::merge_entity_arrays();

  TypedEntityHandle<Marker> h(e);
  CHECK(h.is_valid());

  OptEntity resolved = resolve(h);
  CHECK(resolved.has_value());
  CHECK(resolved->id == e.id);

  // operator-> reaches the entity without a separate resolve step.
  CHECK(h->has<Marker>());

  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();
}

TEST(slices_to_plain_handle_and_still_compares) {
  Entity &e = EntityHelper::createEntity();
  e.addComponent<Marker>();
  EntityHelper::merge_entity_arrays();

  TypedEntityHandle<Marker> typed(e);
  EntityHandle plain = EntityHelper::handle_for(e);

  // Inherits slot/gen rather than wrapping them, so the two are the same
  // handle and ADL finds the base operator==.
  CHECK(typed == plain);
  CHECK(plain == typed);

  // The component type is documentation, not identity: two typed handles to
  // the same entity are equal whatever T says. This entity has no Other, so
  // the constructor logs an expected "entity is missing T" -- that error line
  // is the validator working, not this test failing.
  TypedEntityHandle<Other> differently_typed(plain);
  CHECK(typed == differently_typed);

  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();
}

TEST(stale_handle_does_not_resolve_after_deletion) {
  Entity &e = EntityHelper::createEntity();
  e.addComponent<Marker>();
  EntityHelper::merge_entity_arrays();

  TypedEntityHandle<Marker> h(e);
  CHECK(resolve(h).has_value());

  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();

  // The slot may already be back in the pool; the generation is what makes
  // this answer no.
  CHECK(!resolve(h).has_value());
}

TEST(stale_handle_does_not_alias_the_slots_next_occupant) {
  Entity &first = EntityHelper::createEntity();
  first.addComponent<Marker>();
  EntityHelper::merge_entity_arrays();

  TypedEntityHandle<Marker> stale(first);
  const EntityID first_id = first.id;

  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();

  Entity &second = EntityHelper::createEntity();
  second.addComponent<Marker>();
  EntityHelper::merge_entity_arrays();
  TypedEntityHandle<Marker> live(second);

  CHECK(second.id != first_id);
  CHECK(stale != live);
  CHECK(!resolve(stale).has_value());
  CHECK(resolve(live).has_value());

  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();
}

// ============================================================================
// Main
// ============================================================================

int main() {
  printf("=== typed entity handle tests ===\n\n");

  for (auto &entry : test_registry()) {
    printf("  Running: %s\n", entry.name);
    entry.fn();
  }

  printf("\n%d/%d checks passed\n", tests_passed, tests_run);
  if (tests_passed == tests_run) {
    printf("All checks passed!\n");
    return 0;
  }
  printf("SOME TESTS FAILED!\n");
  return 1;
}
