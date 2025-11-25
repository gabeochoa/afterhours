#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

#define AFTER_HOURS_ENTITY_HELPER
#define AFTER_HOURS_ENTITY_QUERY
#define AFTER_HOURS_SYSTEM
#include "../../ah.h"

// Simple header-only benchmarking utility
namespace bm {
using Clock = std::chrono::high_resolution_clock;
using TimePoint = Clock::time_point;
using Duration = std::chrono::nanoseconds;

struct Result {
  Duration min;
  Duration max;
  Duration mean;
  Duration median;
  size_t iterations;
  size_t samples;
};

class Timer {
public:
  Timer() : start_(Clock::now()) {}
  
  Duration elapsed() const {
    return std::chrono::duration_cast<Duration>(Clock::now() - start_);
  }
  
  double elapsed_ms() const {
    return elapsed().count() / 1e6;
  }
  
  double elapsed_us() const {
    return elapsed().count() / 1e3;
  }

private:
  TimePoint start_;
};

template <typename Func>
Result run_benchmark(const std::string &name, Func &&func, size_t iterations = 100,
                size_t warmup = 10) {
  std::vector<Duration> samples;
  samples.reserve(iterations);

  // Warmup
  for (size_t i = 0; i < warmup; ++i) {
    func();
  }

  // Actual benchmark
  for (size_t i = 0; i < iterations; ++i) {
    Timer t;
    func();
    samples.push_back(t.elapsed());
  }

  // Calculate statistics
  std::sort(samples.begin(), samples.end());
  
  Result result;
  result.samples = samples.size();
  result.iterations = iterations;
  result.min = samples.front();
  result.max = samples.back();
  
  // Mean
  Duration sum{0};
  for (const auto &s : samples) {
    sum += s;
  }
  result.mean = Duration{static_cast<Duration::rep>(sum.count() / static_cast<long long>(samples.size()))};
  
  // Median
  result.median = samples[samples.size() / 2];
  
  // Print results
  std::cout << std::fixed << std::setprecision(2);
  std::cout << name << ":\n";
  std::cout << "  Iterations: " << iterations << "\n";
  std::cout << "  Min:    " << std::setw(10) << result.min.count() / 1e3
            << " us\n";
  std::cout << "  Max:    " << std::setw(10) << result.max.count() / 1e3
            << " us\n";
  std::cout << "  Mean:   " << std::setw(10) << result.mean.count() / 1e3
            << " us\n";
  std::cout << "  Median: " << std::setw(10) << result.median.count() / 1e3
            << " us\n";
  std::cout << "\n";
  
  return result;
}

template <typename Func>
void measure(const std::string &name, Func &&func) {
  Timer t;
  func();
  double elapsed_ms = t.elapsed_ms();
  std::cout << std::fixed << std::setprecision(2);
  std::cout << name << ": " << elapsed_ms << " ms\n";
}
} // namespace bm

using namespace bm;

namespace afterhours {

// Test components
struct Transform : public BaseComponent {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  Transform(float x, float y, float z = 0.0f) : x(x), y(y), z(z) {}
  virtual ~Transform() {}
};

struct Velocity : public BaseComponent {
  float vx = 0.0f;
  float vy = 0.0f;
  Velocity(float vx, float vy) : vx(vx), vy(vy) {}
  virtual ~Velocity() {}
};

struct Health : public BaseComponent {
  int hp = 100;
  int max_hp = 100;
  Health(int hp, int max_hp) : hp(hp), max_hp(max_hp) {}
  virtual ~Health() {}
};

struct Name : public BaseComponent {
  std::string name;
  Name(const std::string &n) : name(n) {}
  virtual ~Name() {}
};

struct Tagged : public BaseComponent {
  int tag = 0;
  Tagged(int t) : tag(t) {}
  virtual ~Tagged() {}
};

// Test systems
struct MoveSystem : System<Transform, Velocity> {
  int count = 0;
  virtual void for_each_with(Entity & /*entity*/, Transform &transform,
                              Velocity &velocity, float dt) override {
    transform.x += velocity.vx * dt;
    transform.y += velocity.vy * dt;
    count++;
  }
};

struct HealthSystem : System<Health> {
  int count = 0;
  virtual void for_each_with(Entity & /*entity*/, Health &health, float /*dt*/) override {
    if (health.hp < health.max_hp) {
      health.hp++;
    }
    count++;
  }
};

} // namespace afterhours

using namespace afterhours;

// Configuration
constexpr size_t NUM_ENTITIES = 10000;
constexpr size_t NUM_COMPONENT_TYPES = 5;
constexpr size_t QUERY_ITERATIONS = 1000;

void create_test_entities(size_t count) {
  std::mt19937 rng(42); // Fixed seed for reproducibility
  std::uniform_real_distribution<float> pos_dist(-100.0f, 100.0f);
  std::uniform_real_distribution<float> vel_dist(-10.0f, 10.0f);
  std::uniform_int_distribution<int> hp_dist(50, 150);
  std::uniform_int_distribution<int> tag_dist(0, 10);

  for (size_t i = 0; i < count; ++i) {
    auto &entity = EntityHelper::createEntity();

    // Always add Transform
    entity.addComponent<Transform>(pos_dist(rng), pos_dist(rng), pos_dist(rng));

    // Randomly add other components
    if (rng() % 2 == 0) {
      entity.addComponent<Velocity>(vel_dist(rng), vel_dist(rng));
    }
    if (rng() % 3 == 0) {
      entity.addComponent<Health>(hp_dist(rng), hp_dist(rng));
    }
    if (rng() % 4 == 0) {
      entity.addComponent<Name>("Entity_" + std::to_string(i));
    }
    if (rng() % 5 == 0) {
      entity.addComponent<Tagged>(tag_dist(rng));
    }

    // Random tags
    if (rng() % 3 == 0) {
      entity.enableTag(1);
    }
    if (rng() % 4 == 0) {
      entity.enableTag(2);
    }
  }
}

void benchmark_entity_creation() {
  std::cout << "=== Entity Creation Benchmark ===\n\n";

  run_benchmark("Create " + std::to_string(NUM_ENTITIES) + " entities",
            []() {
              EntityHelper::delete_all_entities(true);
              create_test_entities(NUM_ENTITIES);
            },
            10, 2);
}

void benchmark_component_access() {
  std::cout << "=== Component Access Benchmark ===\n\n";

  // Reset
  EntityHelper::delete_all_entities(true);
  create_test_entities(NUM_ENTITIES);

  run_benchmark("Access Transform component (direct)", []() {
    int sum = 0;
    for (auto &entity : EntityQuery().whereHasComponent<Transform>().gen()) {
      sum += static_cast<int>(entity.get().get<Transform>().x);
    }
    return sum;
  });

  run_benchmark("Access Transform + Velocity components", []() {
    int sum = 0;
    for (auto &entity :
         EntityQuery()
             .whereHasComponent<Transform>()
             .whereHasComponent<Velocity>()
             .gen()) {
      sum += static_cast<int>(entity.get().get<Transform>().x +
                              entity.get().get<Velocity>().vx);
    }
    return sum;
  });
}

void benchmark_queries() {
  std::cout << "=== Query Performance Benchmark ===\n\n";

  // Reset
  EntityHelper::delete_all_entities(true);
  create_test_entities(NUM_ENTITIES);

  run_benchmark("Query: Transform only", []() {
    return EntityQuery().whereHasComponent<Transform>().gen().size();
  });

  run_benchmark("Query: Transform + Velocity", []() {
    return EntityQuery()
        .whereHasComponent<Transform>()
        .whereHasComponent<Velocity>()
        .gen()
        .size();
  });

  run_benchmark("Query: Transform + Velocity + Health", []() {
    return EntityQuery()
        .whereHasComponent<Transform>()
        .whereHasComponent<Velocity>()
        .whereHasComponent<Health>()
        .gen()
        .size();
  });

  run_benchmark("Query: Transform + Tag 1", []() {
    return EntityQuery()
        .whereHasComponent<Transform>()
        .whereHasTag(1)
        .gen()
        .size();
  });

  run_benchmark("Query: Transform + Tag 1 + Tag 2", []() {
    return EntityQuery()
        .whereHasComponent<Transform>()
        .whereHasAllTags(TagBitset{}.set(1).set(2))
        .gen()
        .size();
  });

  run_benchmark("Query: gen_first()", []() {
    auto opt = EntityQuery()
                   .whereHasComponent<Transform>()
                   .whereHasComponent<Velocity>()
                   .gen_first();
    return opt.has_value();
  });

  run_benchmark("Query: gen_count()", []() {
    return EntityQuery()
        .whereHasComponent<Transform>()
        .whereHasComponent<Velocity>()
        .gen_count();
  });
}

void benchmark_system_iteration() {
  std::cout << "=== System Iteration Benchmark ===\n\n";

  // Reset
  EntityHelper::delete_all_entities(true);
  create_test_entities(NUM_ENTITIES);

  SystemManager systems;
  auto move_system = std::make_unique<MoveSystem>();
  auto health_system = std::make_unique<HealthSystem>();
  systems.register_update_system(std::move(move_system));
  systems.register_update_system(std::move(health_system));

  run_benchmark("System iteration (2 systems)", [&systems]() {
    auto &entities = EntityHelper::get_entities_for_mod();
    systems.tick(entities, 0.016f);
  }, 100, 10);
}

void benchmark_memory_usage() {
  std::cout << "=== Memory Usage Benchmark ===\n\n";

  EntityHelper::delete_all_entities(true);

  measure("Memory: Empty state", []() {
    // Just measure baseline
  });

  create_test_entities(1000);
  measure("Memory: 1,000 entities", []() {});

  create_test_entities(9000); // Total 10k
  measure("Memory: 10,000 entities", []() {});

  create_test_entities(90000); // Total 100k
  measure("Memory: 100,000 entities", []() {});

  std::cout << "\n";
}

void benchmark_query_scaling() {
  std::cout << "=== Query Scaling Benchmark ===\n\n";

  std::vector<size_t> entity_counts = {100, 500, 1000, 5000, 10000, 50000};

  for (size_t count : entity_counts) {
    EntityHelper::delete_all_entities(true);
    create_test_entities(count);

    std::string label = "Query scaling: " + std::to_string(count) + " entities";
    run_benchmark(label, []() {
      return EntityQuery()
          .whereHasComponent<Transform>()
          .whereHasComponent<Velocity>()
          .gen()
          .size();
    }, 100, 5);
  }
}

void print_summary() {
  std::cout << "=== Summary ===\n\n";
  std::cout << "Total entities created: " << NUM_ENTITIES << "\n";
  std::cout << "Component types: " << NUM_COMPONENT_TYPES << "\n";
  std::cout << "Query iterations: " << QUERY_ITERATIONS << "\n";
  std::cout << "\n";
  std::cout << "Use these benchmarks to:\n";
  std::cout << "1. Establish performance baselines before SOA migration\n";
  std::cout << "2. Measure improvements after SOA implementation\n";
  std::cout << "3. Identify performance bottlenecks\n";
  std::cout << "4. Validate query optimizations\n";
  std::cout << "\n";
}

int main(int argc, char **argv) {
  std::cout << "Afterhours ECS Performance Benchmark\n";
  std::cout << "=====================================\n\n";

  bool run_all = true;
  bool run_creation = false;
  bool run_access = false;
  bool run_queries = false;
  bool run_systems = false;
  bool run_memory = false;
  bool run_scaling = false;

  // Parse arguments
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--creation") {
      run_all = false;
      run_creation = true;
    } else if (arg == "--access") {
      run_all = false;
      run_access = true;
    } else if (arg == "--queries") {
      run_all = false;
      run_queries = true;
    } else if (arg == "--systems") {
      run_all = false;
      run_systems = true;
    } else if (arg == "--memory") {
      run_all = false;
      run_memory = true;
    } else if (arg == "--scaling") {
      run_all = false;
      run_scaling = true;
    }
  }

  if (run_all || run_creation) {
    benchmark_entity_creation();
  }
  if (run_all || run_access) {
    benchmark_component_access();
  }
  if (run_all || run_queries) {
    benchmark_queries();
  }
  if (run_all || run_systems) {
    benchmark_system_iteration();
  }
  if (run_all || run_memory) {
    benchmark_memory_usage();
  }
  if (run_all || run_scaling) {
    benchmark_query_scaling();
  }

  print_summary();

  return 0;
}

