// Declared ownership: deleting an owner cascade-deletes what it owns.
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
  if (cond)
    tests_passed++;
  else
    fprintf(stderr, "  FAIL: %s  (%s:%d)\n", expr, file, line);
}
#define CHECK(expr) check((expr), #expr, __FILE__, __LINE__)

static bool alive(EntityCollection &ec, EntityID id) {
  return ec.getEntityForID(id).has_value();
}
static void kill(EntityCollection &ec, EntityID id) {
  auto e = ec.getEntityForID(id);
  CHECK(e.has_value());
  if (e)
    e.asE().cleanup = true;
  ec.cleanup();
}

static void test_transitive_cascade() {
  EntityCollection ec;
  Entity &owner = ec.createEntity();
  Entity &mid = ec.createEntity();
  Entity &leaf = ec.createEntity();
  Entity &other = ec.createEntity();
  ec.declare_ownership(owner, mid);
  ec.declare_ownership(mid.id, leaf.id);
  ec.merge_entity_arrays();
  CHECK(ec.owner_of(leaf.id) == mid.id);
  CHECK(ec.owned_entities(owner.id) == std::vector<EntityID>{mid.id});
  kill(ec, owner.id);
  CHECK(!alive(ec, owner.id));
  CHECK(!alive(ec, mid.id));
  CHECK(!alive(ec, leaf.id));
  CHECK(alive(ec, other.id));
}

static void test_temp_owner_cascades_before_merge() {
  EntityCollection ec;
  Entity &owner = ec.createEntity();
  Entity &owned = ec.createEntity();
  ec.declare_ownership(owner, owned);
  owner.cleanup = true; // flagged while both are still temp
  ec.cleanup();
  CHECK(!alive(ec, owner.id));
  CHECK(!alive(ec, owned.id));
}

static void test_cycles_do_not_hang() {
  EntityCollection ec;
  Entity &a = ec.createEntity();
  Entity &b = ec.createEntity();
  Entity &c = ec.createEntity();
  ec.declare_ownership(a, b);
  ec.declare_ownership(b, a); // cycle
  ec.declare_ownership(b, c);
  ec.merge_entity_arrays();
  kill(ec, a.id);
  CHECK(!alive(ec, a.id));
  CHECK(!alive(ec, b.id));
  CHECK(!alive(ec, c.id));
}

static void test_release_and_redeclare_move() {
  EntityCollection ec;
  Entity &a = ec.createEntity();
  Entity &b = ec.createEntity();
  Entity &owned = ec.createEntity();
  ec.declare_ownership(a, owned);
  CHECK(ec.release_ownership(owned.id));
  CHECK(!ec.release_ownership(owned.id));
  kill(ec, a.id);
  CHECK(alive(ec, owned.id));

  ec.declare_ownership(b, owned); // re-declare moves ownership to b
  CHECK(ec.owner_of(owned.id) == b.id);
  kill(ec, b.id);
  CHECK(!alive(ec, owned.id));
}

static void test_owned_deleted_first_recycled_id_not_inherited() {
  EntityCollection ec;
  Entity &owner = ec.createEntity();
  Entity &owned = ec.createEntity();
  ec.declare_ownership(owner, owned);
  ec.merge_entity_arrays();
  kill(ec, owned.id);
  CHECK(ec.owner_of(owned.id) == -1);
  Entity &fresh = ec.createEntity(); // may reuse owned's recycled id
  ec.merge_entity_arrays();
  kill(ec, owner.id);
  CHECK(alive(ec, fresh.id));
}

int main() {
  printf("Running ownership cascade tests...\n\n");
  printf("  transitive_cascade\n");
  test_transitive_cascade();
  printf("  temp_owner_cascades_before_merge\n");
  test_temp_owner_cascades_before_merge();
  printf("  cycles_do_not_hang\n");
  test_cycles_do_not_hang();
  printf("  release_and_redeclare_move\n");
  test_release_and_redeclare_move();
  printf("  owned_deleted_first_recycled_id_not_inherited\n");
  test_owned_deleted_first_recycled_id_not_inherited();
  printf("\n%d/%d tests passed.\n", tests_passed, tests_run);
  if (tests_passed != tests_run) {
    printf("SOME TESTS FAILED!\n");
    return 1;
  }
  printf("ALL TESTS PASSED.\n");
  return 0;
}
