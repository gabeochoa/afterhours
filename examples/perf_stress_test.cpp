// perf_stress_test.cpp
// Performance stress tests for afterhours subsystems.
//
// Measures wall-clock time for:
//   1. Entity creation & component attachment at scale
//   2. Entity queries with filters (linear scan cost)
//   3. Autolayout with deep/wide trees
//   4. Text measurement cache hit/miss rates
//   5. UI rendering draw-call volume (rounded corners, shadows, text)
//
// Build:
//   make perf_stress_test
//
// Run:
//   ./perf_stress_test              # all benchmarks
//   ./perf_stress_test entity       # just entity benchmarks
//   ./perf_stress_test layout       # just layout benchmarks
//   ./perf_stress_test text_cache   # just text cache benchmarks

#define FMT_HEADER_ONLY
#include <fmt/format.h>

#include <afterhours/src/ecs.h>
#include <afterhours/src/plugins/autolayout.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

using namespace afterhours;
using namespace afterhours::ui;

// ── Timing helpers ──

struct BenchResult {
  std::string name;
  double ms;
  size_t iterations;
};

static std::vector<BenchResult> g_results;

template <typename Fn>
double time_ms(Fn &&fn, size_t iterations = 1) {
  auto t0 = std::chrono::high_resolution_clock::now();
  for (size_t i = 0; i < iterations; ++i)
    fn();
  auto t1 = std::chrono::high_resolution_clock::now();
  return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

static void bench(const char *name, size_t iterations,
                  std::function<void()> fn) {
  fn();
  double ms = time_ms(fn, iterations);
  g_results.push_back({std::string(name), ms, iterations});
  printf("  %-50s %8.2f ms  (%zu iters, %.3f ms/iter)\n", name, ms, iterations,
         ms / static_cast<double>(iterations));
}

// ── Test components ──

struct Position : BaseComponent {
  float x = 0, y = 0;
  Position() = default;
  Position(float x_, float y_) : x(x_), y(y_) {}
};

struct Velocity : BaseComponent {
  float dx = 0, dy = 0;
  Velocity() = default;
  Velocity(float dx_, float dy_) : dx(dx_), dy(dy_) {}
};

struct Health : BaseComponent {
  int hp = 100;
};

struct Tag_Enemy : BaseComponent {};
struct Tag_Player : BaseComponent {};
struct Tag_Projectile : BaseComponent {};

// ── 1. Entity creation & component benchmarks ──

static void bench_entity_creation() {
  printf("\n=== Entity Creation & Components ===\n");

  for (int count : {100, 1000, 5000, 10000}) {
    char name[64];
    snprintf(name, sizeof(name), "create %d entities + 3 components", count);
    bench(name, 3, [count]() {
      EntityCollection collection;
      EntityHelper::set_default_collection(&collection);

      for (int i = 0; i < count; ++i) {
        auto &e = EntityHelper::createEntity();
        e.addComponent<Position>(
            static_cast<float>(i), static_cast<float>(i * 2));
        e.addComponent<Velocity>(1.0f, -1.0f);
        e.addComponent<Health>();
      }
      collection.merge_entity_arrays();

      EntityHelper::set_default_collection(nullptr);
    });
  }
}

// ── 2. Entity query benchmarks ──

static void bench_entity_queries() {
  printf("\n=== Entity Queries ===\n");

  EntityCollection collection;
  EntityHelper::set_default_collection(&collection);

  constexpr int N = 5000;
  for (int i = 0; i < N; ++i) {
    auto &e = EntityHelper::createEntity();
    e.addComponent<Position>(static_cast<float>(i), 0.f);
    if (i % 3 == 0)
      e.addComponent<Velocity>(1.f, 0.f);
    if (i % 5 == 0)
      e.addComponent<Health>();
    if (i % 7 == 0)
      e.addComponent<Tag_Enemy>();
    if (i % 11 == 0)
      e.addComponent<Tag_Player>();
  }
  collection.merge_entity_arrays();

  char name[128];
  snprintf(name, sizeof(name), "query all %d (no filter)", N);
  bench(name, 100, []() {
    EntityQuery<> q;
    auto results = q.gen();
    (void)results.size();
  });

  snprintf(name, sizeof(name), "query whereHasComponent<Velocity> (%d ents)",
           N);
  bench(name, 100, []() {
    EntityQuery<> q;
    q.whereHasComponent<Velocity>();
    auto results = q.gen();
    (void)results.size();
  });

  snprintf(name, sizeof(name),
           "query 2 filters: Velocity + Health (%d ents)", N);
  bench(name, 100, []() {
    EntityQuery<> q;
    q.whereHasComponent<Velocity>();
    q.whereHasComponent<Health>();
    auto results = q.gen();
    (void)results.size();
  });

  snprintf(name, sizeof(name),
           "query 3 filters + lambda (%d ents)", N);
  bench(name, 100, []() {
    EntityQuery<> q;
    q.whereHasComponent<Position>();
    q.whereHasComponent<Velocity>();
    q.whereMissingComponent<Tag_Enemy>();
    q.whereLambda([](const Entity &e) {
      return e.get<Position>().x > 1000.f;
    });
    auto results = q.gen();
    (void)results.size();
  });

  snprintf(name, sizeof(name), "gen_first (stop-on-first) (%d ents)", N);
  bench(name, 1000, []() {
    EntityQuery<> q;
    q.whereHasComponent<Tag_Player>();
    auto result = q.gen_first();
    (void)result.valid();
  });

  snprintf(name, sizeof(name), "gen_count (%d ents)", N);
  bench(name, 100, []() {
    EntityQuery<> q;
    q.whereHasComponent<Health>();
    auto count = q.gen_count();
    (void)count;
  });

  EntityHelper::set_default_collection(nullptr);
}

// ── 3. Autolayout benchmarks ──

struct LayoutTestHarness {
  std::vector<std::unique_ptr<Entity>> entities;
  std::vector<Entity *> mapping;

  Entity &make_entity() {
    entities.push_back(std::make_unique<Entity>());
    Entity &e = *entities.back();
    while (mapping.size() <= static_cast<size_t>(e.id))
      mapping.push_back(nullptr);
    mapping[e.id] = &e;
    return e;
  }

  Entity &make_ui(Size w, Size h, EntityID parent_id = -1) {
    Entity &e = make_entity();
    auto &ui = e.addComponent<UIComponent>(e.id);
    ui.set_desired_width(w);
    ui.set_desired_height(h);
    if (parent_id >= 0) {
      ui.set_parent(parent_id);
      mapping[parent_id]->get<UIComponent>().add_child(e.id);
    }
    return e;
  }

  void layout(Entity &root) {
    AutoLayout::autolayout(root.get<UIComponent>(), {800, 600}, mapping, false);
  }
};

static void bench_autolayout() {
  printf("\n=== Autolayout ===\n");

  for (int children_count : {50, 200, 500, 1000, 2000}) {
    char name[128];
    snprintf(name, sizeof(name), "flat layout: 1 root + %d children",
             children_count);
    bench(name, 20, [children_count]() {
      ENTITY_ID_GEN.store(0);
      LayoutTestHarness h;
      auto &root = h.make_ui(pixels(800), pixels(600));
      root.get<UIComponent>().set_flex_direction(FlexDirection::Column);
      for (int i = 0; i < children_count; ++i)
        h.make_ui(percent(1.f), pixels(20), root.id);
      h.layout(root);
    });
  }

  for (int depth : {10, 50, 100, 200}) {
    char name[128];
    snprintf(name, sizeof(name), "deep layout: depth=%d", depth);
    bench(name, 50, [depth]() {
      ENTITY_ID_GEN.store(0);
      LayoutTestHarness h;
      auto &root = h.make_ui(pixels(800), pixels(600));
      Entity *parent = &root;
      for (int i = 0; i < depth; ++i) {
        auto &child = h.make_ui(percent(0.95f), percent(0.95f), parent->id);
        parent = &child;
      }
      h.layout(root);
    });
  }

  for (int sections : {5, 20, 50}) {
    int items_per = 10;
    char name[128];
    snprintf(name, sizeof(name),
             "realistic: %d sections x %d items = %d widgets", sections,
             items_per, sections * items_per + sections + 1);
    bench(name, 30, [sections, items_per]() {
      ENTITY_ID_GEN.store(0);
      LayoutTestHarness h;
      auto &root = h.make_ui(pixels(800), pixels(600));
      root.get<UIComponent>().set_flex_direction(FlexDirection::Column);
      for (int s = 0; s < sections; ++s) {
        auto &section = h.make_ui(percent(1.f), children(), root.id);
        section.get<UIComponent>().set_flex_direction(FlexDirection::Column);
        for (int i = 0; i < items_per; ++i)
          h.make_ui(percent(1.f), pixels(30), section.id);
      }
      h.layout(root);
    });
  }

  // Layout-only: pre-built entities, measures only AutoLayout::autolayout
  for (int children_count : {200, 1000, 2000}) {
    char name[128];
    snprintf(name, sizeof(name), "layout-only: %d children (no entity alloc)",
             children_count);

    ENTITY_ID_GEN.store(0);
    LayoutTestHarness h;
    auto &root = h.make_ui(pixels(800), pixels(600));
    root.get<UIComponent>().set_flex_direction(FlexDirection::Column);
    for (int i = 0; i < children_count; ++i)
      h.make_ui(percent(1.f), pixels(20), root.id);

    bench(name, 50, [&h, &root]() {
      AutoLayout::autolayout(root.get<UIComponent>(), {800, 600},
                             h.mapping, false);
    });
  }
}

// ── 4. Text measurement cache benchmarks ──

static void bench_text_cache() {
  printf("\n=== Text Measurement Cache ===\n");

  auto fake_measure = [](std::string_view text, std::string_view,
                         float font_size, float) -> Vector2Type {
    // Simulate ~7px per character at base size
    float w = static_cast<float>(text.size()) * 7.0f *
              (font_size / 20.0f);
    return {w, font_size * 1.2f};
  };

  // Cache hit rate with repeated strings
  {
    TextMeasureCache cache(fake_measure);
    constexpr int N = 10000;
    constexpr int UNIQUE = 100;

    // Pre-generate strings
    std::vector<std::string> strings;
    strings.reserve(UNIQUE);
    for (int i = 0; i < UNIQUE; ++i) {
      strings.push_back("Label_" + std::to_string(i));
    }

    bench("text cache: 10k lookups, 100 unique strings", 10, [&]() {
      cache.clear();
      for (int i = 0; i < N; ++i) {
        (void)cache.measure(strings[i % UNIQUE], "default",
                           20.0f + static_cast<float>(i % 5), 1.0f);
      }
    });

    printf("    -> hit rate: %.1f%%, cache size: %zu\n", cache.hit_rate(),
           cache.size());
  }

  // Cache with many unique entries (eviction pressure)
  {
    TextMeasureCache cache(fake_measure);
    cache.set_max_entries(512);

    std::vector<std::string> unique_strings;
    unique_strings.reserve(5000);
    for (int i = 0; i < 5000; ++i)
      unique_strings.push_back("unique_" + std::to_string(i));

    bench("text cache: 5k unique (eviction at 512)", 5, [&]() {
      cache.clear();
      for (int i = 0; i < 5000; ++i) {
        (void)cache.measure(unique_strings[i], "default", 16.0f, 1.0f);
      }
    });
    printf("    -> hit rate: %.1f%%, cache size: %zu\n", cache.hit_rate(),
           cache.size());
  }

  // Hash computation cost
  {
    constexpr int N = 100000;
    std::string test_str = "The quick brown fox jumps over the lazy dog";
    bench("text cache hash: 100k FNV computations", 3, [&]() {
      volatile uint64_t sink = 0;
      for (int i = 0; i < N; ++i) {
        sink = TextMeasureCache::compute_hash(test_str, "default", 20.0f, 1.0f);
      }
      (void)sink;
    });
  }

  // Measure function overhead (simulated)
  {
    TextMeasureCache cache(fake_measure);
    constexpr int N = 10000;

    std::vector<std::string> miss_strings;
    miss_strings.reserve(N);
    for (int i = 0; i < N; ++i)
      miss_strings.push_back("miss_" + std::to_string(i));

    bench("text cache: 10k all-miss (measure fn cost)", 5, [&]() {
      cache.clear();
      for (int i = 0; i < N; ++i) {
        (void)cache.measure(miss_strings[i], "default", 20.0f, 1.0f);
      }
    });
  }
}

// ── 5. Component access patterns ──

static void bench_component_access() {
  printf("\n=== Component Access Patterns ===\n");

  EntityCollection coll;
  EntityHelper::set_default_collection(&coll);

  constexpr int N = 5000;
  for (int i = 0; i < N; ++i) {
    auto &e = coll.createEntity();
    e.addComponent<Position>(static_cast<float>(i), 0.f);
    e.addComponent<Velocity>(1.f, -1.f);
    if (i % 2 == 0)
      e.addComponent<Health>();
    if (i % 3 == 0)
      e.addComponent<Tag_Enemy>();
  }
  coll.merge_entity_arrays();

  char name[128];

  // has<> check cost
  snprintf(name, sizeof(name), "has<> check: %d ents x 4 components", N);
  bench(name, 100, [&coll]() {
    int count = 0;
    for (auto &e : coll.get_entities()) {
      if (!e) continue;
      if (e->has<Position>()) count++;
      if (e->has<Velocity>()) count++;
      if (e->has<Health>()) count++;
      if (e->has<Tag_Enemy>()) count++;
    }
    if (count == -1) printf("unreachable\n");
  });

  // get<> access cost
  snprintf(name, sizeof(name), "get<Position> on %d ents", N);
  bench(name, 100, [&coll]() {
    float sum = 0;
    for (auto &e : coll.get_entities()) {
      if (!e || !e->has<Position>()) continue;
      sum += e->get<Position>().x;
    }
    if (sum == -999.f) printf("unreachable\n");
  });

  // Mixed has+get pattern (typical render loop)
  snprintf(name, sizeof(name),
           "typical render: has+get 3 components (%d ents)", N);
  bench(name, 100, [&coll]() {
    float sum = 0;
    for (auto &e : coll.get_entities()) {
      if (!e) continue;
      if (!e->has<Position>()) continue;
      auto &pos = e->get<Position>();
      if (e->has<Velocity>()) {
        auto &vel = e->get<Velocity>();
        sum += pos.x + vel.dx;
      }
      if (e->has<Health>()) {
        sum += static_cast<float>(e->get<Health>().hp);
      }
    }
    if (sum == -999.f) printf("unreachable\n");
  });

  EntityHelper::set_default_collection(nullptr);
}

// ── Summary ──

static void print_summary() {
  printf("\n");
  printf("========================================");
  printf("========================================\n");
  printf("  SUMMARY\n");
  printf("========================================");
  printf("========================================\n");

  for (auto &r : g_results) {
    double per_iter = r.ms / static_cast<double>(r.iterations);
    printf("  %-50s %8.2f ms total  %.3f ms/iter\n", r.name.c_str(), r.ms,
           per_iter);
  }
  printf("\n");
}

// ── Main ──

int main(int argc, char *argv[]) {
  printf("afterhours performance stress test\n");
  printf("===================================\n");

  bool run_all = (argc < 2);
  auto should_run = [&](const char *name) {
    if (run_all) return true;
    for (int i = 1; i < argc; ++i) {
      if (strcmp(argv[i], name) == 0) return true;
    }
    return false;
  };

  if (should_run("entity"))
    bench_entity_creation();
  if (should_run("query"))
    bench_entity_queries();
  if (should_run("layout"))
    bench_autolayout();
  if (should_run("text_cache"))
    bench_text_cache();
  if (should_run("component"))
    bench_component_access();

  print_summary();
  return 0;
}
