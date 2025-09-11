#include <iostream>
#include "components.h"

using namespace afterhours;

// System that uses Transform - this will get different component type IDs!
struct PhysicsSystem : System<Transform, Velocity> {
  void for_each_with(Entity &entity, Transform &transform, Velocity &velocity, const float dt) override {
    std::cout << "PhysicsSystem: Updating entity " << entity.id 
              << " at position (" << transform.x << ", " << transform.y << ", " << transform.z << ")\n";
    std::cout << "PhysicsSystem: With velocity (" << velocity.vx << ", " << velocity.vy << ", " << velocity.vz << ")\n";
  }
  
  void for_each_with(const Entity &entity, const Transform &transform, const Velocity &velocity, const float dt) const override {
    std::cout << "PhysicsSystem (const): Updating entity " << entity.id 
              << " at position (" << transform.x << ", " << transform.y << ", " << transform.z << ")\n";
    std::cout << "PhysicsSystem (const): With velocity (" << velocity.vx << ", " << velocity.vy << ", " << velocity.vz << ")\n";
  }
};

// Function to show component type IDs from this translation unit
void show_component_ids() {
  std::cout << "From other_file.cpp:\n";
  std::cout << "Transform type ID: " << components::get_type_id<Transform>() << "\n";
  std::cout << "Velocity type ID: " << components::get_type_id<Velocity>() << "\n";
  
  // Create a system to show its bitset
  PhysicsSystem physicsSystem;
  std::cout << "PhysicsSystem component bitset: " << physicsSystem.get_component_bitset() << "\n";
}
