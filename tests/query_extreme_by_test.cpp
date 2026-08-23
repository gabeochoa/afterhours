// query_extreme_by_test.cpp
//
// EntityQuery::gen_min_by / gen_max_by -- argmin/argmax by a projected key.
//
// These exist because the idiom they replace, orderByLambda(cmp).gen_first(),
// materializes every match and sorts it to keep one element. It is O(n log n)
// for an O(n) question, and because std::sort is not stable it does not say
// which of several tied entities you get. The tie tests below are the part
// worth keeping: they pin down the guarantee the sort could not make.
//
// Build (from afterhours/tests/):
//   make query_extreme_by_test
// Run:
//   ./query_extreme_by_test

#define FMT_HEADER_ONLY
#include <fmt/format.h>

#include <afterhours/ah.h>

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
  if (cond) {
    tests_passed++;
  } else {
    fprintf(stderr, "  FAIL: %s  (%s:%d)\n", expr, file, line);
  }
}

#define CHECK(expr) check((expr), #expr, __FILE__, __LINE__)

struct Score : BaseComponent {
  float value;
  Score() : value(0) {}
  Score(float v) : value(v) {}
};

struct Tag : BaseComponent {
  std::string name;
  Tag() = default;
  Tag(std::string n) : name(std::move(n)) {}
};

// Creates a scored entity and merges, so queries see it without force_merge.
static Entity &make_scored(float v, const std::string &name = "") {
  Entity &e = EntityHelper::createEntity();
  e.addComponent<Score>(v);
  if (!name.empty())
    e.addComponent<Tag>(name);
  EntityHelper::merge_entity_arrays();
  return e;
}

static void clear_all() {
  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();
}

static float score_of(const Entity &e) { return e.get<Score>().value; }

// ============================================================================
// Basics
// ============================================================================

TEST(min_and_max_pick_the_extremes) {
  clear_all();
  make_scored(5.f, "five");
  make_scored(1.f, "one");
  make_scored(9.f, "nine");
  make_scored(3.f, "three");

  OptEntity lo = EntityQuery<>().whereHasComponent<Score>().gen_min_by(score_of);
  OptEntity hi = EntityQuery<>().whereHasComponent<Score>().gen_max_by(score_of);

  CHECK(lo.has_value());
  CHECK(hi.has_value());
  CHECK(lo->get<Tag>().name == "one");
  CHECK(hi->get<Tag>().name == "nine");

  clear_all();
}

TEST(empty_result_is_empty_optional) {
  clear_all();
  CHECK(!EntityQuery<>().whereHasComponent<Score>().gen_min_by(score_of));
  CHECK(!EntityQuery<>().whereHasComponent<Score>().gen_max_by(score_of));
  clear_all();
}

TEST(single_match_is_both_min_and_max) {
  clear_all();
  make_scored(42.f, "only");

  OptEntity lo = EntityQuery<>().whereHasComponent<Score>().gen_min_by(score_of);
  OptEntity hi = EntityQuery<>().whereHasComponent<Score>().gen_max_by(score_of);

  CHECK(lo.has_value() && hi.has_value());
  CHECK(lo->id == hi->id);

  clear_all();
}

TEST(no_match_after_filtering_is_empty) {
  clear_all();
  make_scored(1.f);
  make_scored(2.f);

  // A filter that excludes everything must not report an extreme.
  OptEntity none = EntityQuery<>()
                       .whereHasComponent<Score>()
                       .whereLambda([](const Entity &) { return false; })
                       .gen_min_by(score_of);
  CHECK(!none.has_value());

  clear_all();
}

// ============================================================================
// Ties -- the guarantee orderByLambda().gen_first() could not make
// ============================================================================

TEST(ties_return_the_first_match_in_iteration_order) {
  clear_all();
  Entity &first = make_scored(7.f, "first");
  make_scored(7.f, "second");
  make_scored(7.f, "third");

  OptEntity lo = EntityQuery<>().whereHasComponent<Score>().gen_min_by(score_of);
  OptEntity hi = EntityQuery<>().whereHasComponent<Score>().gen_max_by(score_of);

  // Strict comparison in the helper: an equal key never displaces the
  // incumbent, so both ends land on the same, earliest entity.
  CHECK(lo.has_value() && hi.has_value());
  CHECK(lo->id == first.id);
  CHECK(hi->id == first.id);

  clear_all();
}

TEST(tie_at_the_extreme_does_not_displace_the_incumbent) {
  clear_all();
  make_scored(2.f, "middle");
  Entity &best = make_scored(9.f, "best");
  make_scored(9.f, "also_best");

  OptEntity hi = EntityQuery<>().whereHasComponent<Score>().gen_max_by(score_of);
  CHECK(hi.has_value());
  CHECK(hi->get<Tag>().name == "best");
  CHECK(hi->id == best.id);

  clear_all();
}

// ============================================================================
// Composition with the rest of the query surface
// ============================================================================

TEST(respects_where_filters) {
  clear_all();
  make_scored(1.f);           // no Tag
  make_scored(50.f);          // no Tag -- the global max
  make_scored(10.f, "tagged");
  make_scored(20.f, "tagged");

  // The 50 is excluded by the filter, so the max among tagged is 20.
  OptEntity hi = EntityQuery<>()
                     .whereHasComponent<Score>()
                     .whereHasComponent<Tag>()
                     .gen_max_by(score_of);
  CHECK(hi.has_value());
  CHECK(hi->get<Score>().value == 20.f);

  // And the min among tagged is 10, not the untagged 1.
  OptEntity lo = EntityQuery<>()
                     .whereHasComponent<Score>()
                     .whereHasComponent<Tag>()
                     .gen_min_by(score_of);
  CHECK(lo.has_value());
  CHECK(lo->get<Score>().value == 10.f);

  clear_all();
}

TEST(agrees_with_the_order_by_idiom_it_replaces) {
  clear_all();
  make_scored(4.f);
  make_scored(-2.f);
  make_scored(11.f);
  make_scored(0.f);

  OptEntity via_sort = EntityQuery<>()
                           .whereHasComponent<Score>()
                           .orderByLambda([](const Entity &a, const Entity &b) {
                             return a.get<Score>().value >
                                    b.get<Score>().value;
                           })
                           .gen_first();
  OptEntity via_max =
      EntityQuery<>().whereHasComponent<Score>().gen_max_by(score_of);

  CHECK(via_sort.has_value() && via_max.has_value());
  CHECK(via_sort->id == via_max->id);

  clear_all();
}

TEST(an_orderby_on_the_query_does_not_change_the_answer) {
  clear_all();
  make_scored(3.f, "three");
  make_scored(8.f, "eight");
  make_scored(1.f, "one");

  // Ordering ascending would put "one" first; gen_max_by must still say
  // "eight", because the key defines the ordering, not the orderby.
  OptEntity hi = EntityQuery<>()
                     .whereHasComponent<Score>()
                     .orderByLambda([](const Entity &a, const Entity &b) {
                       return a.get<Score>().value < b.get<Score>().value;
                     })
                     .gen_max_by(score_of);
  CHECK(hi.has_value());
  CHECK(hi->get<Tag>().name == "eight");

  clear_all();
}

TEST(key_can_be_a_non_float_comparable) {
  clear_all();
  make_scored(1.f, "charlie");
  make_scored(2.f, "alpha");
  make_scored(3.f, "bravo");

  // std::string keys: exercises the decay_t path, since the projection here
  // returns by value from a member reference.
  OptEntity lo = EntityQuery<>().whereHasComponent<Tag>().gen_min_by(
      [](const Entity &e) { return e.get<Tag>().name; });
  CHECK(lo.has_value());
  CHECK(lo->get<Tag>().name == "alpha");

  clear_all();
}

TEST(key_projection_returning_a_reference_is_handled) {
  clear_all();
  make_scored(5.f, "zulu");
  make_scored(6.f, "alpha");

  // Returns const std::string& -- must not bind a dangling optional.
  OptEntity lo = EntityQuery<>().whereHasComponent<Tag>().gen_min_by(
      [](const Entity &e) -> const std::string & { return e.get<Tag>().name; });
  CHECK(lo.has_value());
  CHECK(lo->get<Tag>().name == "alpha");

  clear_all();
}

// ============================================================================
// orderByMin / orderByMax -- the same key, applied to the whole set
// ============================================================================

TEST(order_by_min_sorts_ascending_and_max_descending) {
  clear_all();
  make_scored(5.f);
  make_scored(1.f);
  make_scored(9.f);
  make_scored(3.f);

  RefEntities asc =
      EntityQuery<>().whereHasComponent<Score>().orderByMin(score_of).gen();
  RefEntities desc =
      EntityQuery<>().whereHasComponent<Score>().orderByMax(score_of).gen();

  CHECK(asc.size() == 4);
  CHECK(desc.size() == 4);
  CHECK(score_of(asc[0]) == 1.f);
  CHECK(score_of(asc[1]) == 3.f);
  CHECK(score_of(asc[2]) == 5.f);
  CHECK(score_of(asc[3]) == 9.f);
  CHECK(score_of(desc[0]) == 9.f);
  CHECK(score_of(desc[1]) == 5.f);
  CHECK(score_of(desc[2]) == 3.f);
  CHECK(score_of(desc[3]) == 1.f);

  clear_all();
}

TEST(order_by_min_first_agrees_with_gen_min_by) {
  clear_all();
  make_scored(4.f, "four");
  make_scored(-2.f, "neg");
  make_scored(11.f, "eleven");

  OptEntity via_order =
      EntityQuery<>().whereHasComponent<Score>().orderByMin(score_of).gen_first();
  OptEntity via_extreme =
      EntityQuery<>().whereHasComponent<Score>().gen_min_by(score_of);
  CHECK(via_order.has_value() && via_extreme.has_value());
  CHECK(via_order->id == via_extreme->id);

  OptEntity via_order_max =
      EntityQuery<>().whereHasComponent<Score>().orderByMax(score_of).gen_first();
  OptEntity via_extreme_max =
      EntityQuery<>().whereHasComponent<Score>().gen_max_by(score_of);
  CHECK(via_order_max.has_value() && via_extreme_max.has_value());
  CHECK(via_order_max->id == via_extreme_max->id);

  clear_all();
}

TEST(order_by_min_composes_with_filters) {
  clear_all();
  make_scored(50.f);           // untagged, would lead if not filtered
  make_scored(30.f, "tagged");
  make_scored(10.f, "tagged");
  make_scored(20.f, "tagged");

  RefEntities tagged = EntityQuery<>()
                           .whereHasComponent<Score>()
                           .whereHasComponent<Tag>()
                           .orderByMax(score_of)
                           .gen();
  CHECK(tagged.size() == 3);
  CHECK(score_of(tagged[0]) == 30.f);
  CHECK(score_of(tagged[2]) == 10.f);

  clear_all();
}

// ============================================================================
// Positional ordering -- each operation applies where it sits in the chain
// ============================================================================

// Same data, same two calls, opposite order: sort-then-take is the top n,
// take-then-sort is the first n found, sorted.
TEST(take_after_sort_is_top_n) {
  clear_all();
  make_scored(30.f, "tagged");
  make_scored(10.f, "tagged");
  make_scored(20.f, "tagged");

  RefEntities two = EntityQuery<>()
                        .whereHasComponent<Score>()
                        .whereHasComponent<Tag>()
                        .orderByMax(score_of)
                        .take(2)
                        .gen();
  CHECK(two.size() == 2);
  CHECK(score_of(two[0]) == 30.f);
  CHECK(score_of(two[1]) == 20.f);

  clear_all();
}

TEST(take_before_sort_is_first_n_then_sorted) {
  clear_all();
  make_scored(30.f, "tagged");
  make_scored(10.f, "tagged");
  make_scored(20.f, "tagged");

  RefEntities two = EntityQuery<>()
                        .whereHasComponent<Score>()
                        .whereHasComponent<Tag>()
                        .take(2)
                        .orderByMax(score_of)
                        .gen();
  CHECK(two.size() == 2);
  // Iteration order keeps 30 and 10; the sort then orders just those two.
  CHECK(score_of(two[0]) == 30.f);
  CHECK(score_of(two[1]) == 10.f);

  clear_all();
}

TEST(take_after_order_by_min_is_bottom_n) {
  clear_all();
  make_scored(30.f);
  make_scored(10.f);
  make_scored(20.f);
  make_scored(5.f);

  RefEntities two = EntityQuery<>()
                        .whereHasComponent<Score>()
                        .orderByMin(score_of)
                        .take(2)
                        .gen();
  CHECK(two.size() == 2);
  CHECK(score_of(two[0]) == 5.f);
  CHECK(score_of(two[1]) == 10.f);

  clear_all();
}

TEST(take_past_the_end_returns_everything_sorted) {
  clear_all();
  make_scored(3.f);
  make_scored(1.f);

  RefEntities all = EntityQuery<>()
                        .whereHasComponent<Score>()
                        .orderByMax(score_of)
                        .take(10)
                        .gen();
  CHECK(all.size() == 2);
  CHECK(score_of(all[0]) == 3.f);
  CHECK(score_of(all[1]) == 1.f);

  clear_all();
}

TEST(take_zero_after_sort_is_empty) {
  clear_all();
  make_scored(3.f);
  make_scored(1.f);

  RefEntities none = EntityQuery<>()
                         .whereHasComponent<Score>()
                         .orderByMax(score_of)
                         .take(0)
                         .gen();
  CHECK(none.empty());

  clear_all();
}

TEST(where_before_the_order_by_filters_first) {
  clear_all();
  make_scored(50.f);           // untagged, would be the top if it survived
  make_scored(30.f, "tagged");
  make_scored(10.f, "tagged");
  make_scored(20.f, "tagged");

  RefEntities two = EntityQuery<>()
                        .whereHasComponent<Tag>()
                        .orderByMax(score_of)
                        .take(2)
                        .gen();
  CHECK(two.size() == 2);
  CHECK(score_of(two[0]) == 30.f);
  CHECK(score_of(two[1]) == 20.f);

  clear_all();
}

TEST(where_after_the_order_by_filters_the_sorted_set) {
  clear_all();
  make_scored(30.f);
  make_scored(10.f);
  make_scored(20.f);
  make_scored(40.f);

  // take(3) keeps the top 3 (40, 30, 20), then the where drops 30.
  RefEntities kept = EntityQuery<>()
                         .whereHasComponent<Score>()
                         .orderByMax(score_of)
                         .take(3)
                         .whereLambda([](const Entity &e) {
                           return score_of(e) != 30.f;
                         })
                         .gen();
  CHECK(kept.size() == 2);
  CHECK(score_of(kept[0]) == 40.f);
  CHECK(score_of(kept[1]) == 20.f);

  clear_all();
}

TEST(order_by_then_take_leaves_gen_first_on_the_sorted_head) {
  clear_all();
  make_scored(30.f);
  make_scored(10.f);
  make_scored(20.f);

  OptEntity top =
      EntityQuery<>().whereHasComponent<Score>().orderByMax(score_of).gen_first();
  CHECK(top.has_value());
  CHECK(score_of(top.asE()) == 30.f);

  clear_all();
}

// ============================================================================
// Main
// ============================================================================

int main() {
  printf("=== query gen_min_by / gen_max_by tests ===\n\n");

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
