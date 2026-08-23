// query_filters_test.cpp
//
// EntityQuery::whereHasComponent<A, B, C> / whereComponentAndLambda<T>(pred) /
// whereFieldHasValue(&T::member, value).
//
// The point of the last two is safety: a whereLambda that calls e.get<X>() is
// only correct if a whereHasComponent<X>() happened to precede it, and nothing
// enforces that. These two make the presence check intrinsic, so the tests
// below care most about what happens to an entity that lacks the component.
//
// Build (from afterhours/tests/):
//   make query_filters_test
// Run:
//   ./query_filters_test

#define FMT_HEADER_ONLY
#include <afterhours/ah.h>
#include <fmt/format.h>

#include <cstdio>
#include <string>
#include <vector>

using namespace afterhours;

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name)                                               \
    static void test_##name();                                   \
    struct Register_##name {                                     \
        Register_##name() { register_test(#name, test_##name); } \
    } register_##name##_instance;                                \
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

struct Alpha : BaseComponent {
    int count;
    bool flag;
    float ratio;
    std::string label;
    Alpha() : count(0), flag(false), ratio(0.f) {}
    Alpha(int c, bool f, float r, std::string l)
        : count(c), flag(f), ratio(r), label(std::move(l)) {}
    bool over(int n) const { return count > n; }
};

struct Beta : BaseComponent {
    int value;
    Beta() : value(0) {}
    Beta(int v) : value(v) {}
};

struct Gamma : BaseComponent {};

static void clear_all() {
    EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();
}

// An entity with no components; the "predicate must not run" cases need these.
static Entity &make_bare() {
    Entity &e = EntityHelper::createEntity();
    EntityHelper::merge_entity_arrays();
    return e;
}

// One entity carrying Alpha with the given fields, plus optional Beta/Gamma.
static Entity &make_alpha(int count, bool flag, float ratio,
                          const std::string &label, bool with_beta = false,
                          bool with_gamma = false) {
    Entity &e = EntityHelper::createEntity();
    e.addComponent<Alpha>(count, flag, ratio, label);
    if (with_beta) e.addComponent<Beta>(count * 10);
    if (with_gamma) e.addComponent<Gamma>();
    EntityHelper::merge_entity_arrays();
    return e;
}

// RefEntities holds reference_wrappers, so ent[i].get<T>() will not parse.
static const std::string &label_of(const Entity &e) {
    return e.get<Alpha>().label;
}
static int count_of(const Entity &e) { return e.get<Alpha>().count; }
static float ratio_of(const Entity &e) { return e.get<Alpha>().ratio; }

// ============================================================================
// Variadic whereHasComponent
// ============================================================================

TEST(variadic_with_one_component_matches_the_single_type_form) {
    clear_all();
    make_alpha(1, false, 0.f, "a");
    make_alpha(2, false, 0.f, "b", true);
    make_bare();

    CHECK(EntityQuery<>().whereHasComponent<Alpha>().gen_count() == 2);
    CHECK(EntityQuery<>().whereHasComponent<Beta>().gen_count() == 1);

    clear_all();
}

TEST(variadic_with_two_components_is_an_and) {
    clear_all();
    make_alpha(1, false, 0.f, "alpha_only");
    make_alpha(2, false, 0.f, "both", true);
    Entity &beta_only = make_bare();
    beta_only.addComponent<Beta>(99);
    EntityHelper::merge_entity_arrays();

    RefEntities both = EntityQuery<>().whereHasComponent<Alpha, Beta>().gen();
    CHECK(both.size() == 1);
    CHECK(label_of(both[0]) == "both");

    clear_all();
}

TEST(variadic_with_three_components_is_an_and) {
    clear_all();
    make_alpha(1, false, 0.f, "ab", true, false);
    make_alpha(2, false, 0.f, "ag", false, true);
    make_alpha(3, false, 0.f, "abg", true, true);

    RefEntities all =
        EntityQuery<>().whereHasComponent<Alpha, Beta, Gamma>().gen();
    CHECK(all.size() == 1);
    CHECK(label_of(all[0]) == "abg");

    clear_all();
}

TEST(missing_any_one_of_the_three_excludes_the_entity) {
    clear_all();
    // Each of these is missing exactly one of the three.
    make_alpha(1, false, 0.f, "no_gamma", true, false);
    make_alpha(2, false, 0.f, "no_beta", false, true);
    Entity &no_alpha = make_bare();
    no_alpha.addComponent<Beta>(1);
    no_alpha.addComponent<Gamma>();
    EntityHelper::merge_entity_arrays();

    const size_t matched =
        EntityQuery<>().whereHasComponent<Alpha, Beta, Gamma>().gen_count();
    CHECK(matched == 0);

    clear_all();
}

TEST(variadic_agrees_with_the_chained_form) {
    clear_all();
    make_alpha(1, false, 0.f, "a");
    make_alpha(2, false, 0.f, "ab", true);
    make_alpha(3, false, 0.f, "abg", true, true);
    make_alpha(4, false, 0.f, "ag", false, true);

    std::vector<int> chained = EntityQuery<>()
                                   .whereHasComponent<Alpha>()
                                   .whereHasComponent<Beta>()
                                   .whereHasComponent<Gamma>()
                                   .gen_ids();
    std::vector<int> variadic =
        EntityQuery<>().whereHasComponent<Alpha, Beta, Gamma>().gen_ids();
    CHECK(chained == variadic);
    CHECK(variadic.size() == 1);

    std::vector<int> chained_pair = EntityQuery<>()
                                        .whereHasComponent<Alpha>()
                                        .whereHasComponent<Beta>()
                                        .gen_ids();
    std::vector<int> variadic_pair =
        EntityQuery<>().whereHasComponent<Alpha, Beta>().gen_ids();
    CHECK(chained_pair == variadic_pair);
    CHECK(variadic_pair.size() == 2);

    clear_all();
}

TEST(variadic_composes_with_other_filters) {
    clear_all();
    make_alpha(5, false, 0.f, "small", true);
    make_alpha(50, false, 0.f, "big", true);
    make_alpha(500, false, 0.f, "no_beta");

    RefEntities kept =
        EntityQuery<>()
            .whereHasComponent<Alpha, Beta>()
            .whereLambda([](const Entity &e) { return count_of(e) > 10; })
            .gen();
    CHECK(kept.size() == 1);
    CHECK(label_of(kept[0]) == "big");

    clear_all();
}

// ============================================================================
// whereComponentAndLambda<T>(pred) -- the predicate never sees a missing
// component
// ============================================================================

TEST(where_component_filters_on_the_component) {
    clear_all();
    make_alpha(1, false, 0.f, "one");
    make_alpha(9, false, 0.f, "nine");

    RefEntities kept = EntityQuery<>()
                           .whereComponentAndLambda<Alpha>(
                               [](const Alpha &a) { return a.over(5); })
                           .gen();
    CHECK(kept.size() == 1);
    CHECK(label_of(kept[0]) == "nine");

    clear_all();
}

TEST(where_component_does_not_invoke_the_predicate_for_missing_components) {
    clear_all();
    make_alpha(1, false, 0.f, "a");
    make_alpha(2, false, 0.f, "b");
    make_bare();
    make_bare();
    make_bare();

    // If the presence check were not intrinsic, this would fire five times --
    // and the three get<Alpha>() calls on entities without one would be UB.
    int calls = 0;
    size_t count = EntityQuery<>()
                       .whereComponentAndLambda<Alpha>([&calls](const Alpha &) {
                           calls++;
                           return true;
                       })
                       .gen_count();
    CHECK(count == 2);
    CHECK(calls == 2);

    clear_all();
}

TEST(where_component_needs_no_preceding_has_check) {
    clear_all();
    make_alpha(3, false, 0.f, "has_beta", true);
    make_alpha(4, false, 0.f, "no_beta");

    // Beta is only on one entity, and nothing guards the read.
    RefEntities kept = EntityQuery<>()
                           .whereComponentAndLambda<Beta>(
                               [](const Beta &b) { return b.value == 30; })
                           .gen();
    CHECK(kept.size() == 1);
    CHECK(label_of(kept[0]) == "has_beta");

    clear_all();
}

TEST(where_component_on_an_absent_component_is_empty) {
    clear_all();
    make_alpha(1, false, 0.f, "a");

    int calls = 0;
    CHECK(EntityQuery<>()
              .whereComponentAndLambda<Beta>([&calls](const Beta &) {
                  calls++;
                  return true;
              })
              .is_empty());
    CHECK(calls == 0);

    clear_all();
}

// ============================================================================
// whereFieldHasValue -- equality on a component member
// ============================================================================

TEST(where_field_on_an_int_member) {
    clear_all();
    make_alpha(7, false, 0.f, "seven");
    make_alpha(8, false, 0.f, "eight");

    RefEntities kept =
        EntityQuery<>().whereFieldHasValue(&Alpha::count, 8).gen();
    CHECK(kept.size() == 1);
    CHECK(label_of(kept[0]) == "eight");

    clear_all();
}

TEST(where_field_on_a_bool_member) {
    clear_all();
    make_alpha(1, true, 0.f, "on");
    make_alpha(2, false, 0.f, "off");
    make_alpha(3, true, 0.f, "also_on");

    CHECK(EntityQuery<>().whereFieldHasValue(&Alpha::flag, true).gen_count() ==
          2);
    CHECK(EntityQuery<>().whereFieldHasValue(&Alpha::flag, false).gen_count() ==
          1);

    clear_all();
}

TEST(where_field_on_a_float_member) {
    clear_all();
    make_alpha(1, false, 0.5f, "half");
    make_alpha(2, false, 1.5f, "one_and_a_half");

    RefEntities kept =
        EntityQuery<>().whereFieldHasValue(&Alpha::ratio, 1.5f).gen();
    CHECK(kept.size() == 1);
    CHECK(label_of(kept[0]) == "one_and_a_half");

    clear_all();
}

TEST(where_field_on_a_string_member) {
    clear_all();
    make_alpha(1, false, 0.f, "alpha");
    make_alpha(2, false, 0.f, "bravo");

    RefEntities kept =
        EntityQuery<>()
            .whereFieldHasValue(&Alpha::label, std::string("bravo"))
            .gen();
    CHECK(kept.size() == 1);
    CHECK(count_of(kept[0]) == 2);

    clear_all();
}

TEST(where_field_with_no_match_is_empty) {
    clear_all();
    make_alpha(1, false, 0.f, "a");
    make_alpha(2, false, 0.f, "b");

    CHECK(EntityQuery<>().whereFieldHasValue(&Alpha::count, 99).is_empty());
    CHECK(EntityQuery<>()
              .whereFieldHasValue(&Alpha::label, std::string("nope"))
              .is_empty());

    clear_all();
}

TEST(where_field_skips_entities_missing_the_component) {
    clear_all();
    make_alpha(1, false, 0.f, "has_beta", true);
    make_alpha(2, false, 0.f, "no_beta");
    make_bare();

    // Beta::value == 10 only on the first; the other two are skipped without
    // their (absent) Beta ever being read.
    RefEntities kept =
        EntityQuery<>().whereFieldHasValue(&Beta::value, 10).gen();
    CHECK(kept.size() == 1);
    CHECK(label_of(kept[0]) == "has_beta");

    // A default-looking value must not match an entity that has no Beta.
    CHECK(EntityQuery<>().whereFieldHasValue(&Beta::value, 0).is_empty());

    clear_all();
}

TEST(where_field_captures_the_value_by_value) {
    clear_all();
    make_alpha(1, false, 0.f, "target");
    make_alpha(2, false, 0.f, "other");

    // The argument is a temporary that dies at the end of the statement that
    // registers the filter; the filter itself runs later, on gen().
    EntityQuery<> q;
    q.whereFieldHasValue(&Alpha::label, std::string("target"));
    RefEntities kept = q.gen();
    CHECK(kept.size() == 1);
    CHECK(count_of(kept[0]) == 1);

    // Same for a scalar read out of a local that is then overwritten.
    int wanted = 2;
    EntityQuery<> q2;
    q2.whereFieldHasValue(&Alpha::count, wanted);
    wanted = 999;
    CHECK(q2.gen_count() == 1);

    clear_all();
}

TEST(where_field_chains_with_itself_and_with_where_component) {
    clear_all();
    make_alpha(5, true, 0.f, "want");
    make_alpha(5, false, 0.f, "wrong_flag");
    make_alpha(6, true, 0.f, "wrong_count");

    RefEntities kept = EntityQuery<>()
                           .whereFieldHasValue(&Alpha::count, 5)
                           .whereFieldHasValue(&Alpha::flag, true)
                           .gen();
    CHECK(kept.size() == 1);
    CHECK(label_of(kept[0]) == "want");

    RefEntities mixed = EntityQuery<>()
                            .whereFieldHasValue(&Alpha::flag, true)
                            .whereComponentAndLambda<Alpha>(
                                [](const Alpha &a) { return a.count == 6; })
                            .gen();
    CHECK(mixed.size() == 1);
    CHECK(label_of(mixed[0]) == "wrong_count");

    clear_all();
}

// ============================================================================
// Composition with ordering and take, including positional ordering
// ============================================================================

TEST(where_field_composes_with_order_by_and_take) {
    clear_all();
    make_alpha(1, true, 30.f, "a");
    make_alpha(1, true, 10.f, "b");
    make_alpha(1, true, 20.f, "c");
    make_alpha(1, false, 99.f, "excluded");

    RefEntities top = EntityQuery<>()
                          .whereFieldHasValue(&Alpha::flag, true)
                          .orderByMax(ratio_of)
                          .take(2)
                          .gen();
    CHECK(top.size() == 2);
    CHECK(label_of(top[0]) == "a");
    CHECK(label_of(top[1]) == "c");

    RefEntities bottom = EntityQuery<>()
                             .whereFieldHasValue(&Alpha::flag, true)
                             .orderByMin(ratio_of)
                             .take(1)
                             .gen();
    CHECK(bottom.size() == 1);
    CHECK(label_of(bottom[0]) == "b");

    clear_all();
}

TEST(where_component_before_the_order_by_filters_the_scan) {
    clear_all();
    make_alpha(1, false, 50.f, "excluded_high");
    make_alpha(1, true, 30.f, "a");
    make_alpha(1, true, 10.f, "b");
    make_alpha(1, true, 20.f, "c");

    RefEntities two = EntityQuery<>()
                          .whereComponentAndLambda<Alpha>(
                              [](const Alpha &a) { return a.flag; })
                          .orderByMax(ratio_of)
                          .take(2)
                          .gen();
    CHECK(two.size() == 2);
    CHECK(label_of(two[0]) == "a");
    CHECK(label_of(two[1]) == "c");

    clear_all();
}

TEST(where_field_after_the_order_by_filters_the_sorted_set) {
    clear_all();
    make_alpha(1, false, 40.f, "top");
    make_alpha(2, false, 30.f, "second");
    make_alpha(3, false, 20.f, "third");
    make_alpha(4, false, 10.f, "fourth");

    // take(3) keeps 40/30/20, then the field filter drops all but count == 2.
    RefEntities kept = EntityQuery<>()
                           .whereHasComponent<Alpha>()
                           .orderByMax(ratio_of)
                           .take(3)
                           .whereFieldHasValue(&Alpha::count, 2)
                           .gen();
    CHECK(kept.size() == 1);
    CHECK(label_of(kept[0]) == "second");

    // The same field filter placed before the sort keeps the same entity, but
    // now take(3) sees only one survivor.
    RefEntities pre = EntityQuery<>()
                          .whereFieldHasValue(&Alpha::count, 2)
                          .orderByMax(ratio_of)
                          .take(3)
                          .gen();
    CHECK(pre.size() == 1);
    CHECK(label_of(pre[0]) == "second");

    clear_all();
}

TEST(where_component_after_the_order_by_filters_the_sorted_set) {
    clear_all();
    make_alpha(1, false, 40.f, "top");
    make_alpha(2, false, 30.f, "second");
    make_alpha(3, false, 20.f, "third");

    RefEntities kept = EntityQuery<>()
                           .whereHasComponent<Alpha>()
                           .orderByMax(ratio_of)
                           .take(2)
                           .whereComponentAndLambda<Alpha>(
                               [](const Alpha &a) { return a.count != 1; })
                           .gen();
    CHECK(kept.size() == 1);
    CHECK(label_of(kept[0]) == "second");

    clear_all();
}

TEST(variadic_composes_with_gen_min_by_and_gen_max_by) {
    clear_all();
    make_alpha(1, false, 99.f, "no_beta");
    make_alpha(2, false, 5.f, "ab_low", true);
    make_alpha(3, false, 15.f, "ab_high", true);

    OptEntity hi =
        EntityQuery<>().whereHasComponent<Alpha, Beta>().gen_max_by(ratio_of);
    OptEntity lo =
        EntityQuery<>().whereHasComponent<Alpha, Beta>().gen_min_by(ratio_of);
    CHECK(hi.has_value() && lo.has_value());
    CHECK(hi->get<Alpha>().label == "ab_high");
    CHECK(lo->get<Alpha>().label == "ab_low");

    clear_all();
}

// ============================================================================
// Main
// ============================================================================

int main() {
    printf(
        "=== query filter tests (variadic has / whereComponentAndLambda / "
        "whereFieldHasValue) "
        "===\n\n");

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
