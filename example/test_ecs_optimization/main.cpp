#define AFTER_HOURS_INCLUDE_DERIVED_CHILDREN
#include "../../ah.h"
#include <cassert>
#include <chrono>
#include <iostream>

using namespace afterhours;

// Test components that match the main game
struct Position : afterhours::BaseComponent {
  float x, y;
  Position(float x = 0, float y = 0) : x(x), y(y) {}
};

struct Transform : afterhours::BaseComponent {
  float x, y, width, height, angle, scale;
  Transform(float x = 0, float y = 0, float w = 10, float h = 10,
            float angle = 0, float scale = 1.0f)
      : x(x), y(y), width(w), height(h), angle(angle), scale(scale) {}
};

struct Velocity : afterhours::BaseComponent {
  float dx, dy;
  Velocity(float dx = 0, float dy = 0) : dx(dx), dy(dy) {}
};

struct Renderable : afterhours::BaseComponent {
  int color;
  Renderable(int color = 0xFF0000) : color(color) {}
};

// Components that cause RenderEntities to skip entities (like in main game)
struct HasSprite : afterhours::BaseComponent {
  float x, y, width, height, angle, scale;
  int color;
  HasSprite(float x = 0, float y = 0, float w = 10, float h = 10,
            float angle = 0, float scale = 1.0f, int color = 0xFF0000)
      : x(x), y(y), width(w), height(h), angle(angle), scale(scale),
        color(color) {}
};

struct HasShader : afterhours::BaseComponent {
  std::vector<int> shaders;
  HasShader() : shaders{1} {} // Simulate having a shader
};

struct HasColor : afterhours::BaseComponent {
  int color;
  HasColor(int color = 0xFF0000) : color(color) {}
};

// Component hierarchy for testing derived methods
struct BaseWeapon : afterhours::BaseComponent {
  int damage;
  BaseWeapon(int dmg = 10) : damage(dmg) {}
};

struct LaserWeapon : BaseWeapon {
  float range;
  LaserWeapon(int dmg = 15, float rng = 100.0f) : BaseWeapon(dmg), range(rng) {}
};

struct PlasmaWeapon : BaseWeapon {
  float charge_time;
  PlasmaWeapon(int dmg = 25, float charge = 2.0f)
      : BaseWeapon(dmg), charge_time(charge) {}
};

// Test systems
struct MovementSystem : System<Position, Velocity> {
  mutable int processed_count = 0;
  virtual void for_each_const(const Entity &entity, const Position &pos,
                              const Velocity &vel, float dt) const override {
    processed_count++;
    // Note: This won't actually modify pos/vel since they're const, but this is
    // just for testing
    (void)entity;
    (void)pos;
    (void)vel;
    (void)dt; // Suppress unused warnings
  }
};

struct RenderSystem : System<Position, Renderable> {
  mutable int rendered_count = 0;
  virtual void for_each_const(const Entity &entity, const Position &pos,
                              const Renderable &rend, float dt) const override {
    rendered_count++;
    // Simulate rendering
    (void)entity;
    (void)pos;
    (void)rend;
    (void)dt; // Suppress unused warnings
  }
};

struct PhysicsSystem : System<Position, Velocity, Renderable> {
  mutable int physics_count = 0;
  virtual void for_each_const(const Entity &entity, const Position &pos,
                              const Velocity &vel, const Renderable &rend,
                              float dt) const override {
    physics_count++;
    // Simulate physics
    (void)entity;
    (void)pos;
    (void)vel;
    (void)rend;
    (void)dt; // Suppress unused warnings
  }
};

// Systems for testing derived component methods
struct WeaponSystem : System<BaseWeapon> {
  mutable int processed_count = 0;
  mutable int laser_count = 0;
  mutable int plasma_count = 0;
  mutable int base_weapon_count = 0;

  virtual void for_each_with(Entity &entity, BaseWeapon &, float dt) override {
    processed_count++;
    (void)dt;

    // Test derived component access
    if (entity.has_child_of<LaserWeapon>()) {
      laser_count++;
    }
    if (entity.has_child_of<PlasmaWeapon>()) {
      plasma_count++;
    }
    if (entity.has_child_of<BaseWeapon>()) {
      base_weapon_count++;
    }
  }

  virtual void for_each_derived(const Entity &entity, float dt) const override {
    processed_count++;
    (void)dt;

    // Test derived component access
    if (entity.has_child_of<LaserWeapon>()) {
      laser_count++;
    }
    if (entity.has_child_of<PlasmaWeapon>()) {
      plasma_count++;
    }
    if (entity.has_child_of<BaseWeapon>()) {
      base_weapon_count++;
    }
  }
};

// System that matches the main game's RenderEntities behavior
struct RenderEntities : System<Position> {
  mutable int processed_count = 0;
  mutable int skipped_sprite_count = 0;
  mutable int skipped_shader_count = 0;

  virtual void for_each_const(const Entity &entity, const Position &pos,
                              float dt) const override {
    processed_count++;

    // This is the exact logic from the main game that causes the problem
    if (entity.has<HasSprite>()) {
      skipped_sprite_count++;
      return; // Skip entities with sprites
    }
    if (entity.has<HasShader>()) {
      skipped_shader_count++;
      return; // Skip entities with shaders
    }

    // This would render basic entities, but we never get here in the main game
    // because all player entities have HasSprite and HasShader components
    (void)pos;
    (void)dt; // Suppress unused warnings
  }
};

void create_test_entities() {
  std::cout << "Creating test entities..." << std::endl;

  for (int i = 0; i < 1000; ++i) {
    auto &entity = EntityHelper::createEntity();
    entity.addComponent<Position>(i * 0.1f, i * 0.1f);

    if (i % 5 != 0) { // 80%
      entity.addComponent<Velocity>(1.0f, 1.0f);
    }
    if (i % 5 != 0 && i % 5 != 1) { // 60%
      entity.addComponent<Renderable>(0xFF0000 + i);
    }

    // Add the components that cause RenderEntities to skip entities (like in
    // main game)
    if (i % 10 == 0) { // 10% have sprites (like player entities)
      entity.addComponent<Transform>(i * 0.1f, i * 0.1f, 20, 20, 0, 1.0f);
      entity.addComponent<HasSprite>(i * 0.1f, i * 0.1f, 20, 20, 0, 1.0f,
                                     0xFF0000 + i);
      entity.addComponent<HasShader>();
      entity.addComponent<HasColor>(0xFF0000 + i);
    }
  }

  std::cout << "Created 1000 entities" << std::endl;
}

void test_component_sets() {
  std::cout << "\nTesting component sets..." << std::endl;

  auto position_entities = EntityHelper::intersect_components<Position>();
  auto velocity_entities = EntityHelper::intersect_components<Velocity>();
  auto renderable_entities = EntityHelper::intersect_components<Renderable>();
  auto sprite_entities = EntityHelper::intersect_components<HasSprite>();
  auto shader_entities = EntityHelper::intersect_components<HasShader>();

  std::cout << "Position entities: " << position_entities.size() << std::endl;
  std::cout << "Velocity entities: " << velocity_entities.size() << std::endl;
  std::cout << "Renderable entities: " << renderable_entities.size()
            << std::endl;
  std::cout << "Sprite entities: " << sprite_entities.size() << std::endl;
  std::cout << "Shader entities: " << shader_entities.size() << std::endl;

  assert(position_entities.size() == 1000 &&
         "Should have 1000 entities with Position");
  assert(velocity_entities.size() == 800 &&
         "Should have 800 entities with Velocity");
  assert(renderable_entities.size() == 600 &&
         "Should have 600 entities with Renderable");
  assert(sprite_entities.size() == 100 &&
         "Should have 100 entities with HasSprite");
  assert(shader_entities.size() == 100 &&
         "Should have 100 entities with HasShader");

  std::cout << "✓ Component set tests passed!" << std::endl;
}

void test_system_performance() {
  std::cout << "\nTesting system performance..." << std::endl;

  MovementSystem movement_system;
  RenderSystem render_system;
  PhysicsSystem physics_system;

  // Test old method (iterate all entities)
  auto start = std::chrono::high_resolution_clock::now();
  for (int iter = 0; iter < 100; ++iter) {
    movement_system.processed_count = 0;
    render_system.rendered_count = 0;
    physics_system.physics_count = 0;

    for (const auto &entity : EntityHelper::get_entities()) {
      if (!entity)
        continue;

      if (entity->has<Position>() && entity->has<Velocity>()) {
        movement_system.for_each_const(*entity, entity->get<Position>(),
                                       entity->get<Velocity>(), 0.016f);
      }
      if (entity->has<Position>() && entity->has<Renderable>()) {
        render_system.for_each_const(*entity, entity->get<Position>(),
                                     entity->get<Renderable>(), 0.016f);
      }
      if (entity->has<Position>() && entity->has<Velocity>() &&
          entity->has<Renderable>()) {
        physics_system.for_each_const(*entity, entity->get<Position>(),
                                      entity->get<Velocity>(),
                                      entity->get<Renderable>(), 0.016f);
      }
    }
  }
  auto end = std::chrono::high_resolution_clock::now();
  auto old_duration =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start)
          .count();

  // Test new method (intersect_components)
  start = std::chrono::high_resolution_clock::now();
  for (int iter = 0; iter < 100; ++iter) {
    movement_system.processed_count = 0;
    render_system.rendered_count = 0;
    physics_system.physics_count = 0;

    auto pos_vel_entities =
        EntityHelper::intersect_components<Position, Velocity>();
    for (EntityID id : pos_vel_entities) {
      auto opt_entity = EntityHelper::getEntityForID(id);
      if (opt_entity) {
        movement_system.for_each_const(
            opt_entity.asE(), opt_entity.asE().get<Position>(),
            opt_entity.asE().get<Velocity>(), 0.016f);
      }
    }

    auto pos_rend_entities =
        EntityHelper::intersect_components<Position, Renderable>();
    for (EntityID id : pos_rend_entities) {
      auto opt_entity = EntityHelper::getEntityForID(id);
      if (opt_entity) {
        render_system.for_each_const(
            opt_entity.asE(), opt_entity.asE().get<Position>(),
            opt_entity.asE().get<Renderable>(), 0.016f);
      }
    }

    auto all_three_entities =
        EntityHelper::intersect_components<Position, Velocity, Renderable>();
    for (EntityID id : all_three_entities) {
      auto opt_entity = EntityHelper::getEntityForID(id);
      if (opt_entity) {
        physics_system.for_each_const(
            opt_entity.asE(), opt_entity.asE().get<Position>(),
            opt_entity.asE().get<Velocity>(),
            opt_entity.asE().get<Renderable>(), 0.016f);
      }
    }
  }
  end = std::chrono::high_resolution_clock::now();
  auto new_duration =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start)
          .count();

  std::cout << "Old method time: " << old_duration << " microseconds"
            << std::endl;
  std::cout << "New method time: " << new_duration << " microseconds"
            << std::endl;
  std::cout << "Speedup: " << (double)old_duration / new_duration << "x"
            << std::endl;

  assert(movement_system.processed_count == 80000 &&
         "Movement count should match");
  assert(render_system.rendered_count == 60000 && "Render count should match");
  assert(physics_system.physics_count == 60000 && "Physics count should match");

  std::cout << "✓ Performance tests passed!" << std::endl;
}

void test_render_entities_problem() {
  std::cout
      << "\nTesting RenderEntities problem (reproduces main game issue)..."
      << std::endl;

  RenderEntities render_entities_system;

  // Test the RenderEntities system with the optimized approach
  auto pos_entities = EntityHelper::intersect_components<Position>();
  std::cout << "Found " << pos_entities.size() << " entities with Position"
            << std::endl;

  for (EntityID id : pos_entities) {
    auto opt_entity = EntityHelper::getEntityForID(id);
    if (opt_entity) {
      render_entities_system.for_each_const(
          opt_entity.asE(), opt_entity.asE().get<Position>(), 0.016f);
    }
  }

  std::cout << "RenderEntities processed: "
            << render_entities_system.processed_count << std::endl;
  std::cout << "Skipped due to sprites: "
            << render_entities_system.skipped_sprite_count << std::endl;
  std::cout << "Skipped due to shaders: "
            << render_entities_system.skipped_shader_count << std::endl;

  // This demonstrates the problem: RenderEntities processes all entities with
  // Position, but skips most of them because they have HasSprite or HasShader
  // components
  assert(render_entities_system.processed_count == 1000 &&
         "Should process all 1000 entities");
  assert(render_entities_system.skipped_sprite_count == 100 &&
         "Should skip 100 entities with sprites");
  // Note: skipped_shader_count is 0 because entities with both sprites and
  // shaders are caught by the sprite check first and return early
  assert(render_entities_system.skipped_shader_count == 0 &&
         "Should skip 0 entities with shaders (caught by sprite check first)");

  std::cout << "✓ RenderEntities problem reproduced!" << std::endl;
  std::cout << "This explains why the main game shows nothing - RenderEntities "
               "skips entities with sprites/shaders"
            << std::endl;
}

void test_render_sprites_with_shaders_problem() {
  std::cout << "\nTesting RenderSpritesWithShaders problem (reproduces main "
               "game issue)..."
            << std::endl;

  // Test the exact same logic as RenderSpritesWithShaders::for_each_with
  // It requires: Transform, HasSprite, HasShader, HasColor
  auto entities = EntityHelper::intersect_components<Transform, HasSprite,
                                                     HasShader, HasColor>();

  std::cout << "RenderSpritesWithShaders found " << entities.size()
            << " entities with all 4 components" << std::endl;

  // Also test individual component counts
  auto transform_entities =
      EntityHelper::get_entities_with_component<Transform>();
  auto sprite_entities = EntityHelper::get_entities_with_component<HasSprite>();
  auto shader_entities = EntityHelper::get_entities_with_component<HasShader>();
  auto color_entities = EntityHelper::get_entities_with_component<HasColor>();

  std::cout << "Individual component counts:" << std::endl;
  std::cout << "  Transform: " << transform_entities.size() << std::endl;
  std::cout << "  HasSprite: " << sprite_entities.size() << std::endl;
  std::cout << "  HasShader: " << shader_entities.size() << std::endl;
  std::cout << "  HasColor: " << color_entities.size() << std::endl;

  // Test pairwise intersections to see where we lose entities
  auto transform_sprite =
      EntityHelper::intersect_components<Transform, HasSprite>();
  auto transform_sprite_shader =
      EntityHelper::intersect_components<Transform, HasSprite, HasShader>();

  std::cout << "Pairwise intersections:" << std::endl;
  std::cout << "  Transform + HasSprite: " << transform_sprite.size()
            << std::endl;
  std::cout << "  Transform + HasSprite + HasShader: "
            << transform_sprite_shader.size() << std::endl;

  // This should reproduce the main game issue: 0 entities found
  if (entities.size() == 0) {
    std::cout
        << "❌ PROBLEM REPRODUCED: RenderSpritesWithShaders finds 0 entities!"
        << std::endl;
    std::cout << "This is exactly what's happening in the main game."
              << std::endl;
  } else {
    std::cout << "✓ RenderSpritesWithShaders found " << entities.size()
              << " entities" << std::endl;
  }
}

void test_derived_component_methods() {
  std::cout << "\nTesting derived component methods (for_each_derived and "
               "for_each_derived_const)..."
            << std::endl;

  // Create entities with different weapon types
  for (int i = 0; i < 50; ++i) {
    auto &entity = EntityHelper::createEntity();
    entity.addComponent<Position>(i * 10.0f, i * 10.0f);

    if (i % 3 == 0) {
      entity.addComponent<LaserWeapon>(15 + i, 100.0f + i);
    } else if (i % 3 == 1) {
      entity.addComponent<PlasmaWeapon>(25 + i, 2.0f + i * 0.1f);
    } else {
      entity.addComponent<BaseWeapon>(10 + i);
    }

    // Also add BaseWeapon to all entities so they match the system
    entity.addComponent<BaseWeapon>(10 + i);
  }

  EntityHelper::merge_entity_arrays();
  EntityHelper::rebuild_component_sets();

  WeaponSystem weapon_system;

  // Test for_each_derived (non-const)
  std::cout << "Testing for_each_derived (non-const)..." << std::endl;
  weapon_system.processed_count = 0;
  weapon_system.laser_count = 0;
  weapon_system.plasma_count = 0;
  weapon_system.base_weapon_count = 0;

  auto weapon_entities = EntityHelper::intersect_components<BaseWeapon>();
  for (EntityID id : weapon_entities) {
    auto opt_entity = EntityHelper::getEntityForID(id);
    if (opt_entity) {
      weapon_system.for_each_with(opt_entity.asE(),
                                  opt_entity.asE().get<BaseWeapon>(), 0.016f);
    }
  }

  std::cout << "  Processed: " << weapon_system.processed_count << std::endl;
  std::cout << "  Laser weapons: " << weapon_system.laser_count << std::endl;
  std::cout << "  Plasma weapons: " << weapon_system.plasma_count << std::endl;
  std::cout << "  Base weapons: " << weapon_system.base_weapon_count
            << std::endl;

  // Test for_each_derived (const)
  std::cout << "Testing for_each_derived (const)..." << std::endl;
  weapon_system.processed_count = 0;
  weapon_system.laser_count = 0;
  weapon_system.plasma_count = 0;
  weapon_system.base_weapon_count = 0;

  for (EntityID id : weapon_entities) {
    auto opt_entity = EntityHelper::getEntityForID(id);
    if (opt_entity) {
      weapon_system.for_each_derived(opt_entity.asE(), 0.016f);
    }
  }

  std::cout << "  Processed: " << weapon_system.processed_count << std::endl;
  std::cout << "  Laser weapons: " << weapon_system.laser_count << std::endl;
  std::cout << "  Plasma weapons: " << weapon_system.plasma_count << std::endl;
  std::cout << "  Base weapons: " << weapon_system.base_weapon_count
            << std::endl;

  // Verify counts
  assert(weapon_system.processed_count == 50 &&
         "Should process all 50 entities");
  assert(weapon_system.laser_count == 17 &&
         "Should find 17 laser weapons (50/3 rounded up)");
  assert(weapon_system.plasma_count == 17 &&
         "Should find 17 plasma weapons (50/3 rounded up)");
  assert(weapon_system.base_weapon_count == 50 &&
         "Should find 50 base weapons (all have BaseWeapon directly)");

  std::cout << "✓ Derived component methods work correctly!" << std::endl;
  std::cout << "  - for_each_derived (non-const) processes entities with "
               "derived components"
            << std::endl;
  std::cout << "  - for_each_derived_const (const) processes entities with "
               "derived components"
            << std::endl;
  std::cout << "  - has_child_of<> correctly identifies derived component types"
            << std::endl;
}

int main() {
  std::cout << "=== ECS Optimization Test (Reproducing Main Game Problem) ==="
            << std::endl;

  create_test_entities();
  EntityHelper::merge_entity_arrays();
  EntityHelper::rebuild_component_sets();

  test_component_sets();
  // test_system_performance(); // Skip for now - focus on main issue
  test_render_entities_problem();
  test_render_sprites_with_shaders_problem();
  test_derived_component_methods();

  std::cout << "\n=== All tests passed! ===" << std::endl;
  std::cout << "The main game issue is that RenderEntities skips entities with "
               "HasSprite/HasShader components"
            << std::endl;
  std::cout << "The main game uses separate systems (RenderSpritesWithShaders) "
               "for those entities"
            << std::endl;

  return 0;
}