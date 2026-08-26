// cleanup() must not reorder the entities it keeps.
//
// UI tab order IS this order: process_tabbing hands focus to the next entity
// the iteration reaches, so a cleanup that reshuffles survivors reshuffles
// where Tab goes. Nothing removed a UI entity mid-session until the retirement
// sweep, which is why swap-with-back went unnoticed.
#define FMT_HEADER_ONLY
#include <fmt/format.h>

#include <afterhours/ah.h>

#include <cstdio>
#include <vector>

using namespace afterhours;

static int tests_run = 0;
static int tests_passed = 0;

static void check(bool cond, const char *expr, const char *file, int line) {
  tests_run++;
  if (cond) {
    tests_passed++;
  } else {
    fprintf(stderr, "  FAIL: %s  (%s:%d)\n", expr, file, line);
  }
}

#define CHECK(expr) check((expr), #expr, __FILE__, __LINE__)

static std::vector<EntityID> live_ids(EntityCollection &ec) {
  std::vector<EntityID> out;
  for (const auto &sp : ec.get_entities_for_mod())
    if (sp)
      out.push_back(sp->id);
  return out;
}

// Removing from the middle must leave the rest in the order they were made.
static void test_survivors_keep_their_order() {
  EntityCollection ec;
  std::vector<EntityID> made;
  for (int i = 0; i < 8; i++)
    made.push_back(ec.createEntity().id);
  ec.merge_entity_arrays();

  // Drop 2, 3 and 6 -- interior, so swap-with-back would pull the tail forward.
  for (int idx : {2, 3, 6})
    for (auto &sp : ec.get_entities_for_mod())
      if (sp && sp->id == made[(size_t)idx])
        sp->cleanup = true;
  ec.cleanup();

  const std::vector<EntityID> want = {made[0], made[1], made[4],
                                      made[5], made[7]};
  CHECK(live_ids(ec) == want);
}

// The tail is a separate path from the interior; drop both ends too.
static void test_edges_and_runs() {
  EntityCollection ec;
  std::vector<EntityID> made;
  for (int i = 0; i < 6; i++)
    made.push_back(ec.createEntity().id);
  ec.merge_entity_arrays();

  for (int idx : {0, 4, 5})
    for (auto &sp : ec.get_entities_for_mod())
      if (sp && sp->id == made[(size_t)idx])
        sp->cleanup = true;
  ec.cleanup();

  const std::vector<EntityID> want = {made[1], made[2], made[3]};
  CHECK(live_ids(ec) == want);
}

static void test_removing_everything_is_empty() {
  EntityCollection ec;
  for (int i = 0; i < 4; i++)
    ec.createEntity();
  ec.merge_entity_arrays();
  for (auto &sp : ec.get_entities_for_mod())
    if (sp)
      sp->cleanup = true;
  ec.cleanup();
  CHECK(live_ids(ec).empty());
}

static void test_removing_nothing_changes_nothing() {
  EntityCollection ec;
  std::vector<EntityID> made;
  for (int i = 0; i < 5; i++)
    made.push_back(ec.createEntity().id);
  ec.merge_entity_arrays();
  ec.cleanup();
  CHECK(live_ids(ec) == made);
}

int main() {
  printf("Running cleanup order tests...\n\n");
  printf("  survivors_keep_their_order\n");
  test_survivors_keep_their_order();
  printf("  edges_and_runs\n");
  test_edges_and_runs();
  printf("  removing_everything_is_empty\n");
  test_removing_everything_is_empty();
  printf("  removing_nothing_changes_nothing\n");
  test_removing_nothing_changes_nothing();

  printf("\n%d/%d tests passed.\n", tests_passed, tests_run);
  if (tests_passed != tests_run) {
    printf("SOME TESTS FAILED!\n");
    return 1;
  }
  printf("ALL TESTS PASSED.\n");
  return 0;
}
