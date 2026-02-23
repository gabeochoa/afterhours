// entity_mapping_test.cpp
//
// Tests for entity ID uniqueness and the vector-based entity mapping cache
// used by BuildUIEntityMapping.
//
// Key bugs tested:
//   - ENTITY_ID_GEN must be `inline` (not `static`) so all translation units
//     share a single counter.  With `static`, each TU gets its own counter
//     starting from 0, causing entity ID collisions when entities are created
//     from multiple .cpp files.
//   - The vector-based mapping cache (UIEntityMappingCache) must produce
//     correct O(1) lookups for all entity IDs.
//
// Build (from afterhours/examples/):
//   make entity_mapping_test
// Run:
//   ./entity_mapping_test

#define FMT_HEADER_ONLY
#include <fmt/format.h>

#include <afterhours/ah.h>
#include <afterhours/src/plugins/autolayout.h>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "entity_mapping_test_helper.h"

using namespace afterhours;
using namespace afterhours::ui;

// ============================================================================
// Test helpers (same pattern as autolayout_test.cpp)
// ============================================================================

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

// ============================================================================
// Mapping cache: replicates the logic from BuildUIEntityMapping::once()
// in systems.h.  We test the algorithm directly, without the full ECS
// system manager, same way autolayout_test.cpp tests layout.
// ============================================================================

struct MappingCache {
  std::vector<Entity *> components;

  Entity &to_ent(EntityID id) {
    if (id < 0 || static_cast<size_t>(id) >= components.size() ||
        components[id] == nullptr) {
      fprintf(stderr, "MappingCache: entity %d not in mapping\n", id);
      std::abort();
    }
    return *components[id];
  }

  UIComponent &to_cmp(EntityID id) {
    return to_ent(id).template get<UIComponent>();
  }
};

struct MappingTest {
  std::vector<std::unique_ptr<Entity>> entities;

  Entity &make_entity() {
    entities.push_back(std::make_unique<Entity>());
    return *entities.back();
  }

  Entity &make_ui_entity() {
    Entity &e = make_entity();
    e.addComponent<UIComponent>(e.id);
    return e;
  }

  // Build a mapping cache using the same algorithm as BuildUIEntityMapping.
  MappingCache build_cache() {
    MappingCache cache;

    EntityID max_id = 0;
    bool any_ui = false;
    for (auto &e : entities) {
      if (!e->has<UIComponent>())
        continue;
      max_id = std::max(max_id, e->id);
      any_ui = true;
    }

    if (!any_ui)
      return cache;

    cache.components.assign(static_cast<size_t>(max_id) + 1, nullptr);
    for (auto &e : entities) {
      if (!e->has<UIComponent>())
        continue;
      cache.components[e->id] = e.get();
    }
    return cache;
  }
};

// ============================================================================
// Tests: Entity ID uniqueness
// ============================================================================

// ---------------------------------------------------------------------------
// Entity IDs created in THIS TU should be sequential and unique.
// ---------------------------------------------------------------------------
TEST(entity_ids_unique_in_single_tu) {
  std::set<int> ids;
  for (int i = 0; i < 100; ++i) {
    auto e = std::make_unique<Entity>();
    CHECK(ids.find(e->id) == ids.end());
    ids.insert(e->id);
  }
  CHECK(ids.size() == 100);
}

// ---------------------------------------------------------------------------
// ENTITY_ID_GEN must be the SAME object across all translation units.
// When it's `static`, each TU gets its own copy → different addresses.
// When it's `inline`, all TUs share one instance → same address.
// ---------------------------------------------------------------------------
TEST(entity_id_gen_shared_across_translation_units) {
  std::atomic_int *local_addr = &afterhours::ENTITY_ID_GEN;
  std::atomic_int *remote_addr = get_entity_id_gen_address_from_other_tu();

  CHECK(local_addr == remote_addr);
}

// ============================================================================
// Tests: Mapping cache correctness
// ============================================================================

// ---------------------------------------------------------------------------
// Basic mapping: create N entities, build cache, verify each is retrievable.
// ---------------------------------------------------------------------------
TEST(mapping_cache_basic) {
  MappingTest t;
  Entity &e1 = t.make_ui_entity();
  Entity &e2 = t.make_ui_entity();
  Entity &e3 = t.make_ui_entity();

  auto cache = t.build_cache();

  CHECK(cache.components.size() > static_cast<size_t>(e3.id));
  CHECK(cache.components[e1.id] == &e1);
  CHECK(cache.components[e2.id] == &e2);
  CHECK(cache.components[e3.id] == &e3);
}

// ---------------------------------------------------------------------------
// to_ent() should return the correct entity reference.
// ---------------------------------------------------------------------------
TEST(mapping_cache_to_ent) {
  MappingTest t;
  Entity &e1 = t.make_ui_entity();
  Entity &e2 = t.make_ui_entity();

  auto cache = t.build_cache();

  CHECK(&cache.to_ent(e1.id) == &e1);
  CHECK(&cache.to_ent(e2.id) == &e2);
}

// ---------------------------------------------------------------------------
// to_cmp() should return the correct UIComponent.
// ---------------------------------------------------------------------------
TEST(mapping_cache_to_cmp) {
  MappingTest t;
  Entity &e = t.make_ui_entity();

  auto cache = t.build_cache();

  UIComponent &cmp = cache.to_cmp(e.id);
  CHECK(cmp.id == e.id);
}

// ---------------------------------------------------------------------------
// Only UIComponent entities should be mapped; plain entities are skipped.
// ---------------------------------------------------------------------------
TEST(mapping_cache_skips_non_ui_entities) {
  MappingTest t;
  Entity &ui = t.make_ui_entity();
  Entity &plain = t.make_entity();

  auto cache = t.build_cache();

  CHECK(cache.components[ui.id] == &ui);
  if (static_cast<size_t>(plain.id) < cache.components.size()) {
    CHECK(cache.components[plain.id] == nullptr);
  }
}

// ---------------------------------------------------------------------------
// Empty entity set: cache should be empty.
// ---------------------------------------------------------------------------
TEST(mapping_cache_empty) {
  MappingTest t;
  auto cache = t.build_cache();
  CHECK(cache.components.empty());
}

// ---------------------------------------------------------------------------
// Single entity: minimal mapping.
// ---------------------------------------------------------------------------
TEST(mapping_cache_single_entity) {
  MappingTest t;
  Entity &e = t.make_ui_entity();

  auto cache = t.build_cache();

  CHECK(cache.components.size() == static_cast<size_t>(e.id) + 1);
  CHECK(cache.components[e.id] == &e);
}

// ---------------------------------------------------------------------------
// Many entities: stress test.
// ---------------------------------------------------------------------------
TEST(mapping_cache_many_entities) {
  MappingTest t;
  std::vector<Entity *> expected;
  for (int i = 0; i < 200; ++i) {
    expected.push_back(&t.make_ui_entity());
  }

  auto cache = t.build_cache();

  for (Entity *e : expected) {
    CHECK(static_cast<size_t>(e->id) < cache.components.size());
    CHECK(cache.components[e->id] == e);
  }
}

// ---------------------------------------------------------------------------
// Rebuilding the cache (simulating multiple frames) replaces old data.
// ---------------------------------------------------------------------------
TEST(mapping_cache_rebuild_replaces_old_data) {
  MappingTest t;
  Entity &e1 = t.make_ui_entity();

  auto cache = t.build_cache();
  CHECK(cache.components[e1.id] == &e1);

  // Add another entity and rebuild
  Entity &e2 = t.make_ui_entity();
  cache = t.build_cache();

  CHECK(cache.components[e1.id] == &e1);
  CHECK(cache.components[e2.id] == &e2);
}

// ---------------------------------------------------------------------------
// When ENTITY_ID_GEN is properly shared (inline), entities created in
// different TUs get non-colliding IDs and can coexist in the same mapping.
// ---------------------------------------------------------------------------
TEST(mapping_cache_with_cross_tu_entities) {
  MappingTest t;
  Entity &local1 = t.make_ui_entity();
  Entity &local2 = t.make_ui_entity();

  // Create entities in another TU.  With a shared counter, their IDs
  // continue from where local entities left off → no collisions.
  std::vector<int> remote_ids = create_entities_in_other_tu(5);

  for (int rid : remote_ids) {
    CHECK(local1.id != rid);
    CHECK(local2.id != rid);
  }

  auto cache = t.build_cache();
  CHECK(cache.components[local1.id] == &local1);
  CHECK(cache.components[local2.id] == &local2);
}

// ============================================================================
// Main
// ============================================================================

int main() {
  printf("Running entity mapping tests...\n\n");

  for (auto &entry : test_registry()) {
    printf("  [%s]\n", entry.name);
    entry.fn();
  }

  printf("\n%d/%d checks passed", tests_passed, tests_run);
  if (tests_passed == tests_run) {
    printf(" -- ALL OK\n");
  } else {
    printf(" -- %d FAILED\n", tests_run - tests_passed);
  }

  return (tests_passed == tests_run) ? 0 : 1;
}
