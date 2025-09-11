#include <iostream>
#include "components.h"
#include "header.h"

using namespace afterhours;

// System that uses Transform
struct RenderSystem : System<Transform> {
  void for_each_with(Entity &entity, Transform &transform, const float dt) override {
    std::cout << "RenderSystem: Rendering entity " << entity.id 
              << " at position (" << transform.x << ", " << transform.y << ", " << transform.z << ")\n";
  }
  
  void for_each_with(const Entity &entity, const Transform &transform, const float dt) const override {
    std::cout << "RenderSystem (const): Rendering entity " << entity.id 
              << " at position (" << transform.x << ", " << transform.y << ", " << transform.z << ")\n";
  }
};

int main() {
  std::cout << "=== Translation Unit Test ===\n";
  std::cout << "This test demonstrates the static initialization order fiasco\n";
  std::cout << "across multiple translation units.\n\n";
  
  // Show component type IDs from main.cpp
  std::cout << "From main.cpp:\n";
  std::cout << "Transform type ID: " << components::get_type_id<Transform>() << "\n";
  std::cout << "Velocity type ID: " << components::get_type_id<Velocity>() << "\n";
  
  // Create entities
  Entity &entity1 = EntityHelper::createEntity();
  entity1.addComponent<Transform>(1.0f, 2.0f, 3.0f);
  
  Entity &entity2 = EntityHelper::createEntity();
  entity2.addComponent<Transform>(4.0f, 5.0f, 6.0f);
  entity2.addComponent<Velocity>(0.1f, 0.2f, 0.3f);
  
  EntityHelper::merge_entity_arrays();
  
  // Test system
  RenderSystem renderSystem;
  std::cout << "\nRenderSystem component bitset: " << renderSystem.get_component_bitset() << "\n";
  
  // Test system calls
  std::cout << "\nTesting system calls:\n";
  renderSystem.for_each(entity1, 0.016f);
  renderSystem.for_each(entity2, 0.016f);
  
  // Show component IDs from other translation unit
  std::cout << "\n";
  show_component_ids();
  
  std::cout << "\n=== SOLUTION DEMONSTRATED ===\n";
  std::cout << "By defining components in a single translation unit (components.h),\n";
  std::cout << "all systems now use the same component type IDs!\n";
  std::cout << "This ensures that systems can find entities because their component\n";
  std::cout << "bitsets match what's stored in the system map.\n";
  std::cout << "\nTest completed!\n";
  return 0;
}
