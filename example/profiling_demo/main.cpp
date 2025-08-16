#include "../../ah.h"
#include <chrono>
#include <iostream>
#include <random>
#include <thread>

namespace afterhours {

// Define some components for our demo
struct Transform : public BaseComponent {
  float x, y;
  Transform(float x, float y) : x(x), y(y) {}
  virtual ~Transform() = default;
};

struct Velocity : public BaseComponent {
  float dx, dy;
  Velocity(float dx, float dy) : dx(dx), dy(dy) {}
  virtual ~Velocity() = default;
};

struct Health : public BaseComponent {
  float current;
  float max;
  Health(float max_health) : current(max_health), max(max_health) {}
  virtual ~Health() = default;
};

struct Player : public BaseComponent {
  std::string name;
  Player(const std::string &n) : name(n) {}
  virtual ~Player() = default;
};

// Create some systems to profile
struct MovementSystem : System<Transform, Velocity> {
  virtual void for_each_with(Entity &entity, Transform &transform,
                             Velocity &velocity, float dt) override {
    PROFILE_SCOPE("MovementSystem::for_each_with");

    // Simulate some work
    transform.x += velocity.dx * dt;
    transform.y += velocity.dy * dt;

    // Add some computation to make it measurable
    for (int i = 0; i < 100; ++i) {
      volatile float temp = std::sin(transform.x) * std::cos(transform.y);
      (void)temp;
    }
  }
};

struct HealthSystem : System<Health> {
  virtual void for_each_with(Entity &entity, Health &health,
                             float dt) override {
    PROFILE_SCOPE("HealthSystem::for_each_with");

    // Simulate health regeneration
    if (health.current < health.max) {
      health.current += 1.0f * dt;
      if (health.current > health.max) {
        health.current = health.max;
      }
    }

    // Add some computation
    for (int i = 0; i < 50; ++i) {
      volatile float temp = std::sqrt(health.current * health.max);
      (void)temp;
    }
  }
};

struct PlayerSystem : System<Player, Transform, Health> {
  virtual void for_each_with(Entity &entity, Player &player,
                             Transform &transform, Health &health,
                             float dt) override {
    PROFILE_SCOPE("PlayerSystem::for_each_with");

    // Simulate player logic
    if (health.current < health.max * 0.5f) {
      // Player is low on health, maybe trigger some effect
      volatile float danger_level = (health.max - health.current) / health.max;
      (void)danger_level;
    }

    // Add some computation
    for (int i = 0; i < 75; ++i) {
      volatile float temp = std::pow(transform.x, 2) + std::pow(transform.y, 2);
      (void)temp;
    }
  }
};

// Custom entity query for profiling
struct DemoQuery : public EntityQuery<DemoQuery> {
  struct WhereNearOrigin : EntityQuery::Modification {
    float max_distance;
    explicit WhereNearOrigin(float distance) : max_distance(distance) {}

    bool operator()(const Entity &entity) const override {
      PROFILE_SCOPE("WhereNearOrigin::operator()");
      if (!entity.has<Transform>())
        return false;

      const auto &transform = entity.get<Transform>();
      float distance =
          std::sqrt(transform.x * transform.x + transform.y * transform.y);
      return distance <= max_distance;
    }
  };

  DemoQuery &whereNearOrigin(float max_distance) {
    return add_mod(new WhereNearOrigin(max_distance));
  }
};

} // namespace afterhours

void create_test_entities() {
  PROFILE_SCOPE("create_test_entities");

  using namespace afterhours;

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<float> pos_dist(-100.0f, 100.0f);
  std::uniform_real_distribution<float> vel_dist(-10.0f, 10.0f);
  std::uniform_real_distribution<float> health_dist(50.0f, 200.0f);

  // Create entities with different component combinations
  for (int i = 0; i < 100; ++i) {
    auto &entity = EntityHelper::createEntity();
    entity.addComponent<Transform>(pos_dist(gen), pos_dist(gen));

    if (i % 3 == 0) {
      entity.addComponent<Velocity>(vel_dist(gen), vel_dist(gen));
    }

    if (i % 2 == 0) {
      entity.addComponent<Health>(health_dist(gen));
    }

    if (i % 5 == 0) {
      entity.addComponent<Player>("Player_" + std::to_string(i));
    }
  }

  // Merge entities to make them available
  EntityHelper::merge_entity_arrays();
}

void run_systems() {
  PROFILE_SCOPE("run_systems");

  // Example of profiling with system type names
  PROFILE_SCOPE_SYSTEM_TYPE(afterhours::MovementSystem, "custom_profile");

  // Register systems
  auto movement_system = std::make_unique<afterhours::MovementSystem>();
  auto health_system = std::make_unique<afterhours::HealthSystem>();
  auto player_system = std::make_unique<afterhours::PlayerSystem>();

  // Run systems (this would normally be done by SystemManager)
  movement_system->once(0.016f);
  health_system->once(0.016f);
  player_system->once(0.016f);
}

void run_queries() {
  PROFILE_SCOPE("run_queries");

  using namespace afterhours;

  // Test different query patterns
  {
    PROFILE_SCOPE("basic_queries");
    auto all_entities = EntityQuery().gen();
    auto entities_with_transform =
        EntityQuery().whereHasComponent<Transform>().gen();
    auto entities_with_health = EntityQuery().whereHasComponent<Health>().gen();

    std::cout << "Total entities: " << all_entities.size() << std::endl;
    std::cout << "Entities with Transform: " << entities_with_transform.size()
              << std::endl;
    std::cout << "Entities with Health: " << entities_with_health.size()
              << std::endl;
  }

  {
    PROFILE_SCOPE("custom_query");
    auto near_origin = DemoQuery().whereNearOrigin(50.0f).gen();
    std::cout << "Entities near origin: " << near_origin.size() << std::endl;
  }

  {
    PROFILE_SCOPE("complex_query");
    auto complex_result = EntityQuery()
                              .whereHasComponent<Transform>()
                              .whereHasComponent<Velocity>()
                              .whereHasComponent<Health>()
                              .take(10)
                              .gen();
    std::cout << "Complex query result: " << complex_result.size() << std::endl;
  }
}

int main() {
  std::cout << "AfterHours Enhanced Profiling Demo\n";
  std::cout << "==================================\n\n";

  // Initialize profiler
  if (!profiling::g_profiler.init_file("profile_demo.spall")) {
    std::cerr << "Failed to initialize profiler!\n";
    return 1;
  }

  std::cout << "Profiler initialized successfully!\n";
  std::cout << "Profiling data will be written to: profile_demo.spall\n\n";

  // Create test entities
  std::cout << "Creating test entities...\n";
  create_test_entities();

  // Run multiple iterations to get good profiling data
  for (int iteration = 0; iteration < 5; ++iteration) {
    std::cout << "Iteration " << (iteration + 1) << "/5\n";

    PROFILE_SCOPE_ARGS("main_iteration",
                       "iteration=" + std::to_string(iteration));

    // Run systems
    run_systems();

    // Run queries
    run_queries();

    // Flush profiling data periodically
    PROFILE_FLUSH();

    // Small delay to make iterations distinct
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  // Shutdown profiler
  profiling::g_profiler.shutdown();

  std::cout << "\nProfiling completed!\n";
  std::cout << "You can now open 'profile_demo.spall' in a spall viewer\n";
  std::cout << "like https://github.com/colrdavidson/spall-web\n";

  return 0;
}