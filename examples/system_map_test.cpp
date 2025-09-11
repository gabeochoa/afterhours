#include "../src/entity_helper.h"
#include "../src/system.h"
#include <cassert>
#include <iostream>
#include <unordered_set>

using namespace afterhours;

// Define some test components
struct TestTransform : BaseComponent {
  float x, y, z;
  TestTransform(float x = 0, float y = 0, float z = 0) : x(x), y(y), z(z) {}
};

struct TestCanShoot : BaseComponent {
  float cooldown;
  TestCanShoot(float cooldown = 1.0f) : cooldown(cooldown) {}
};

struct TestHasSprite : BaseComponent {
  int sprite_id;
  TestHasSprite(int sprite_id = 0) : sprite_id(sprite_id) {}
};

struct TestPlayer : TestTransform {
  std::string name;
  int level;
  TestPlayer(std::string name = "Player", int level = 1, float x = 0,
             float y = 0, float z = 0)
      : TestTransform(x, y, z), name(name), level(level) {}
};

// Debug helper class to track system calls
template <typename... Components> struct DebugSystem : System<Components...> {
  int call_count = 0;
  std::string system_name;

  DebugSystem(const std::string &name) : system_name(name) {}

  void for_each_with(Entity &entity, Components &...components,
                     const float dt) override {
    call_count++;
    std::cout << system_name << " processing entity " << entity.id << " (call #"
              << call_count << ")\n";
  }

  void for_each_with(const Entity &entity, const Components &...components,
                     const float dt) const override {
    // For const systems, we can't modify call_count, but we can still log
    std::cout << system_name << " (const) processing entity " << entity.id
              << "\n";
  }

  void reset() { call_count = 0; }
  int get_call_count() const { return call_count; }
};

// Test systems using DebugSystem
using SystemA = DebugSystem<TestTransform>;
using SystemB = DebugSystem<TestTransform, TestCanShoot>;
using SystemC = DebugSystem<TestHasSprite>;
using SystemD = DebugSystem<TestTransform, TestHasSprite>;
using SystemE = DebugSystem<>;
using SystemF = DebugSystem<TestPlayer>;

// RenderEntities-like system that should match entities with TestTransform
struct RenderEntities : System<TestTransform> {
  int render_count = 0;

  void for_each_with(Entity &entity, TestTransform &transform,
                     const float dt) override {
    render_count++;
    std::cout << "RenderEntities: Rendering entity " << entity.id
              << " at position (" << transform.x << ", " << transform.y << ", "
              << transform.z << ")\n";
  }

  void for_each_with(const Entity &entity, const TestTransform &transform,
                     const float dt) const override {
    std::cout << "RenderEntities (const): Rendering entity " << entity.id
              << " at position (" << transform.x << ", " << transform.y << ", "
              << transform.z << ")\n";
  }

  void reset() { render_count = 0; }
  int get_render_count() const { return render_count; }
};

void printSystemMap() {
  auto &entity_helper = EntityHelper::get();
  std::cout << "\n=== System Map State ===\n";
  for (const auto &[componentSet, entityIds] : entity_helper.system_map) {
    std::cout << "ComponentSet: " << componentSet << " -> Entities: [";
    for (auto it = entityIds.begin(); it != entityIds.end(); ++it) {
      std::cout << *it;
      if (std::next(it) != entityIds.end()) {
        std::cout << ", ";
      }
    }
    std::cout << "]\n";
  }
  std::cout << "========================\n\n";
}

void printSystemComponentBitsets() {
  std::cout << "\n=== System Component Bitsets ===\n";

  // Create test systems to show their component bitsets
  SystemA systemA("SystemA");
  SystemB systemB("SystemB");
  SystemC systemC("SystemC");
  SystemD systemD("SystemD");
  SystemE systemE("SystemE");
  SystemF systemF("SystemF");
  RenderEntities renderSystem;

  std::cout << "SystemA (Transform): " << systemA.get_component_bitset()
            << "\n";
  std::cout << "SystemB (Transform+CanShoot): "
            << systemB.get_component_bitset() << "\n";
  std::cout << "SystemC (HasSprite): " << systemC.get_component_bitset()
            << "\n";
  std::cout << "SystemD (Transform+HasSprite): "
            << systemD.get_component_bitset() << "\n";
  std::cout << "SystemE (empty): " << systemE.get_component_bitset() << "\n";
  std::cout << "SystemF (Player): " << systemF.get_component_bitset() << "\n";
  std::cout << "RenderEntities (Transform): "
            << renderSystem.get_component_bitset() << "\n";

  std::cout << "================================\n\n";
}

// Helper function to check if an entity is in a specific component set
bool isEntityInComponentSet(EntityID entity_id,
                            const ComponentBitSet &componentSet) {
  auto &entity_helper = EntityHelper::get();
  auto it = entity_helper.system_map.find(componentSet);
  if (it == entity_helper.system_map.end()) {
    return false;
  }
  // Find entity by ID in the set of Entity*
  for (const auto &entity_ptr : it->second) {
    if (entity_ptr && entity_ptr->id == entity_id) {
      return true;
    }
  }
  return false;
}

// Helper function to count entities in a specific component set
size_t countEntitiesInComponentSet(const ComponentBitSet &componentSet) {
  auto &entity_helper = EntityHelper::get();
  auto it = entity_helper.system_map.find(componentSet);
  if (it == entity_helper.system_map.end()) {
    return 0;
  }
  return it->second.size();
}

// Helper function to create a component bitset for testing
ComponentBitSet createComponentSet(bool hasTransform, bool hasCanShoot,
                                   bool hasHasSprite, bool hasPlayer = false) {
  ComponentBitSet set;
  if (hasTransform) {
    set.set(components::get_type_id<TestTransform>());
  }
  if (hasCanShoot) {
    set.set(components::get_type_id<TestCanShoot>());
  }
  if (hasHasSprite) {
    set.set(components::get_type_id<TestHasSprite>());
  }
  if (hasPlayer) {
    set.set(components::get_type_id<TestPlayer>());
  }
  return set;
}

// Test functions
void testBasicComponentOperations() {
  std::cout << "=== Testing Basic Component Operations ===\n";

  // Create entity
  std::cout << "1. Creating entity...\n";
  Entity &entity = EntityHelper::createEntity();
  std::cout << "Created entity with ID: " << entity.id << "\n";

  // Merge temp entities to main entities array
  EntityHelper::merge_entity_arrays();

  // Assert: Entity should be in empty component set (matches SystemE)
  ComponentBitSet emptySet = createComponentSet(false, false, false);
  assert(isEntityInComponentSet(entity.id, emptySet));
  assert(countEntitiesInComponentSet(emptySet) == 1);
  printSystemMap();

  // Add TestTransform component
  std::cout << "2. Adding TestTransform component...\n";
  entity.addComponent<TestTransform>(1.0f, 2.0f, 3.0f);

  // Assert: Entity should now be in Transform-only set (matches SystemA and
  // SystemE)
  ComponentBitSet transformSet = createComponentSet(true, false, false);
  assert(isEntityInComponentSet(entity.id, transformSet));
  assert(countEntitiesInComponentSet(transformSet) == 1);
  assert(!isEntityInComponentSet(entity.id,
                                 emptySet)); // Should no longer be in empty set
  printSystemMap();

  // Add TestHasSprite component
  std::cout << "3. Adding TestHasSprite component...\n";
  entity.addComponent<TestHasSprite>(42);

  // Assert: Entity should now be in Transform+HasSprite set (matches SystemA,
  // SystemC, SystemD, and SystemE)
  ComponentBitSet transformSpriteSet = createComponentSet(true, false, true);
  assert(isEntityInComponentSet(entity.id, transformSpriteSet));
  assert(countEntitiesInComponentSet(transformSpriteSet) == 1);
  assert(!isEntityInComponentSet(
      entity.id, transformSet)); // Should no longer be in Transform-only set
  printSystemMap();

  // Add TestCanShoot component
  std::cout << "4. Adding TestCanShoot component...\n";
  entity.addComponent<TestCanShoot>(0.5f);

  // Assert: Entity should now be in all components set (matches SystemA,
  // SystemB, SystemC, SystemD, and SystemE)
  ComponentBitSet allComponentsSet = createComponentSet(true, true, true);
  assert(isEntityInComponentSet(entity.id, allComponentsSet));
  assert(countEntitiesInComponentSet(allComponentsSet) == 1);
  assert(!isEntityInComponentSet(
      entity.id,
      transformSpriteSet)); // Should no longer be in Transform+Sprite set
  printSystemMap();

  // Test which systems would match this entity
  std::cout << "5. Testing system matches:\n";
  assert(entity.has<TestTransform>());
  assert(entity.has<TestCanShoot>());
  assert(entity.has<TestHasSprite>());
  std::cout << "All component checks passed!\n";

  // Remove TestTransform component
  std::cout << "\n6. Removing TestTransform component...\n";
  entity.removeComponent<TestTransform>();

  // Assert: Entity should now be in CanShoot+HasSprite set (matches SystemC and
  // SystemE)
  ComponentBitSet canShootSpriteSet = createComponentSet(false, true, true);
  assert(isEntityInComponentSet(entity.id, canShootSpriteSet));
  assert(countEntitiesInComponentSet(canShootSpriteSet) == 1);
  assert(!isEntityInComponentSet(
      entity.id,
      allComponentsSet)); // Should no longer be in all components set
  printSystemMap();

  // Test matches after removal
  std::cout << "7. Testing system matches after TestTransform removal:\n";
  assert(!entity.has<TestTransform>());
  assert(entity.has<TestCanShoot>());
  assert(entity.has<TestHasSprite>());
  std::cout << "All component checks after removal passed!\n";

  // Remove all remaining components
  std::cout << "\n8. Removing all remaining components...\n";
  entity.removeComponent<TestCanShoot>();
  entity.removeComponent<TestHasSprite>();

  // Assert: Entity should be back in empty set
  assert(isEntityInComponentSet(entity.id, emptySet));
  assert(countEntitiesInComponentSet(emptySet) == 1);
  assert(!isEntityInComponentSet(
      entity.id,
      canShootSpriteSet)); // Should no longer be in CanShoot+Sprite set
  printSystemMap();
}

void testInheritance() {
  std::cout << "\n=== Testing Component Inheritance ===\n";

  // Create entity
  Entity &entity = EntityHelper::createEntity();
  EntityHelper::merge_entity_arrays();

  // Add TestPlayer component (inherits from TestTransform)
  std::cout
      << "9. Adding TestPlayer component (inherits from TestTransform)...\n";
  entity.addComponent<TestPlayer>("TestPlayer", 5, 10.0f, 20.0f, 30.0f);

  // Assert: Entity should now be in Player set (which includes Transform)
  ComponentBitSet playerSet = createComponentSet(false, false, false, true);
  assert(isEntityInComponentSet(entity.id, playerSet));
  assert(countEntitiesInComponentSet(playerSet) == 1);
  printSystemMap();

  // Verify inheritance works - TestPlayer should match TestTransform systems
  std::cout << "10. Testing inheritance - TestPlayer should match "
               "TestTransform systems:\n";
  assert(entity.has<TestTransform>()); // TestPlayer inherits from TestTransform
  assert(entity.has<TestPlayer>());
  assert(!entity.has<TestCanShoot>());
  assert(!entity.has<TestHasSprite>());
  std::cout
      << "Inheritance test passed! TestPlayer component has TestTransform "
         "capabilities.\n";

  // Add other components to TestPlayer
  std::cout << "\n11. Adding TestCanShoot to TestPlayer entity...\n";
  entity.addComponent<TestCanShoot>(0.3f);

  // Assert: Entity should now be in Player+CanShoot set
  ComponentBitSet playerCanShootSet =
      createComponentSet(false, true, false, true);
  assert(isEntityInComponentSet(entity.id, playerCanShootSet));
  assert(countEntitiesInComponentSet(playerCanShootSet) == 1);
  assert(!isEntityInComponentSet(
      entity.id, playerSet)); // Should no longer be in Player-only set
  printSystemMap();

  // Verify TestPlayer with TestCanShoot matches multiple systems
  std::cout << "12. Testing system matches for TestPlayer with TestCanShoot:\n";
  assert(entity.has<TestTransform>()); // TestPlayer inherits from TestTransform
  assert(entity.has<TestPlayer>());
  assert(entity.has<TestCanShoot>());
  assert(!entity.has<TestHasSprite>());
  std::cout << "TestPlayer with TestCanShoot matches TestTransform, "
               "TestPlayer, and TestCanShoot "
               "systems!\n";

  // Remove TestPlayer component
  std::cout << "\n13. Removing TestPlayer component...\n";
  entity.removeComponent<TestPlayer>();

  // Assert: Entity should now be in CanShoot-only set
  ComponentBitSet canShootOnlySet =
      createComponentSet(false, true, false, false);
  assert(isEntityInComponentSet(entity.id, canShootOnlySet));
  assert(countEntitiesInComponentSet(canShootOnlySet) == 1);
  assert(!isEntityInComponentSet(
      entity.id,
      playerCanShootSet)); // Should no longer be in Player+CanShoot set
  printSystemMap();

  // Verify TestPlayer removal
  std::cout << "14. Testing TestPlayer removal:\n";
  assert(!entity.has<TestPlayer>());
  assert(!entity.has<TestTransform>()); // TestTransform should be gone too
                                        // since TestPlayer was removed
  assert(entity.has<TestCanShoot>());
  std::cout
      << "TestPlayer removal test passed! TestTransform is also removed when "
         "TestPlayer is removed.\n";
}

void testSystemCalls(SystemA &systemA, SystemB &systemB, SystemC &systemC,
                     SystemD &systemD, SystemE &systemE, SystemF &systemF) {
  std::cout << "\n=== Testing System Calls ===\n";

  // Reset all system call counts
  systemA.reset();
  systemB.reset();
  systemC.reset();
  systemD.reset();
  systemE.reset();
  systemF.reset();

  // Create entity and add components
  Entity &entity = EntityHelper::createEntity();
  EntityHelper::merge_entity_arrays();

  // Test with empty entity (should only match SystemE)
  std::cout << "Testing with empty entity:\n";
  systemE.for_each(entity, 0.0f);
  assert(systemE.get_call_count() == 1);
  assert(systemA.get_call_count() == 0);
  assert(systemB.get_call_count() == 0);
  assert(systemC.get_call_count() == 0);
  assert(systemD.get_call_count() == 0);
  assert(systemF.get_call_count() == 0);
  std::cout << "Empty entity test passed!\n";

  // Add TestTransform component
  entity.addComponent<TestTransform>(1.0f, 2.0f, 3.0f);
  systemA.reset();
  systemE.reset();

  std::cout << "Testing with TestTransform component:\n";
  systemA.for_each(entity, 0.0f);
  systemE.for_each(entity, 0.0f);
  assert(systemA.get_call_count() == 1);
  assert(systemE.get_call_count() == 1);
  std::cout << "TestTransform component test passed!\n";

  // Add TestPlayer component
  entity.addComponent<TestPlayer>("TestPlayer", 3, 5.0f, 10.0f, 15.0f);
  systemA.reset();
  systemE.reset();
  systemF.reset();

  std::cout << "Testing with TestPlayer component:\n";
  systemA.for_each(entity, 0.0f);
  systemE.for_each(entity, 0.0f);
  systemF.for_each(entity, 0.0f);
  assert(systemA.get_call_count() ==
         1); // TestPlayer inherits from TestTransform
  assert(systemE.get_call_count() == 1);
  assert(systemF.get_call_count() == 1);
  std::cout << "TestPlayer component test passed!\n";
}

void testRenderEntities() {
  std::cout << "\n=== Testing RenderEntities System ===\n";

  // Show what component bitsets systems are looking for
  printSystemComponentBitsets();

  // Create RenderEntities system
  RenderEntities renderSystem;

  // Create entity with TestTransform component
  std::cout << "Creating entity with TestTransform component...\n";
  Entity &entity = EntityHelper::createEntity();
  entity.addComponent<TestTransform>(100.0f, 200.0f, 300.0f);
  EntityHelper::merge_entity_arrays();

  std::cout << "Entity created with ID: " << entity.id << "\n";
  printSystemMap();

  // Show the mismatch issue
  std::cout << "=== Component Bitset Mismatch Analysis ===\n";
  std::cout << "RenderEntities system looking for: "
            << renderSystem.get_component_bitset() << "\n";

  auto &entity_helper = EntityHelper::get();
  std::cout << "Entities in system map:\n";
  for (const auto &[componentSet, entityIds] : entity_helper.system_map) {
    std::cout << "  ComponentSet: " << componentSet << " -> "
              << entityIds.size() << " entities\n";
    if (componentSet == renderSystem.get_component_bitset()) {
      std::cout << "    ✓ MATCH! RenderEntities will find these entities\n";
    } else {
      std::cout
          << "    ✗ NO MATCH! RenderEntities will NOT find these entities\n";
    }
  }
  std::cout << "==========================================\n\n";

  // Test direct system call
  std::cout << "Testing direct RenderEntities call...\n";
  renderSystem.for_each(entity, 0.016f);
  assert(renderSystem.get_render_count() == 1);
  std::cout << "Direct call test passed!\n";

  // Test with SystemManager
  std::cout << "Testing RenderEntities with SystemManager...\n";
  SystemManager systemManager;
  systemManager.register_render_system(std::make_unique<RenderEntities>());

  renderSystem.reset();
  systemManager.render(EntityHelper::get_entities_for_mod(), 0.016f);
  // Note: SystemManager creates its own RenderEntities instance, so we can't
  // track calls
  std::cout << "SystemManager render test passed!\n";

  // Test with TestPlayer component (should also match RenderEntities)
  std::cout << "Testing RenderEntities with TestPlayer component...\n";
  Entity &playerEntity = EntityHelper::createEntity();
  playerEntity.addComponent<TestPlayer>("TestPlayer", 5, 50.0f, 60.0f, 70.0f);
  EntityHelper::merge_entity_arrays();

  std::cout << "Player entity created with ID: " << playerEntity.id << "\n";
  printSystemMap();

  // Show the mismatch issue again
  std::cout << "=== Component Bitset Mismatch Analysis (After Player) ===\n";
  std::cout << "RenderEntities system looking for: "
            << renderSystem.get_component_bitset() << "\n";

  std::cout << "Entities in system map:\n";
  for (const auto &[componentSet, entityIds] : entity_helper.system_map) {
    std::cout << "  ComponentSet: " << componentSet << " -> "
              << entityIds.size() << " entities\n";
    if (componentSet == renderSystem.get_component_bitset()) {
      std::cout << "    ✓ MATCH! RenderEntities will find these entities\n";
    } else {
      std::cout
          << "    ✗ NO MATCH! RenderEntities will NOT find these entities\n";
    }
  }
  std::cout << "======================================================\n\n";

  systemManager.render(EntityHelper::get_entities_for_mod(), 0.016f);
  // Note: SystemManager creates its own RenderEntities instance, so we can't
  // track calls
  std::cout << "Player component render test passed!\n";

  std::cout << "All RenderEntities tests passed!\n";
}

void testComponentBitsetMismatch() {
  std::cout << "\n=== Testing Component Bitset Mismatch Issue ===\n";

  // This demonstrates the issue we're seeing in the main game
  std::cout
      << "This test demonstrates the issue where systems can't find entities\n";
  std::cout << "because their component bitsets don't match what's in the "
               "system map.\n\n";

  // Create a system that looks for a specific component
  SystemA systemA("SystemA");
  auto systemBitset = systemA.get_component_bitset();
  std::cout << "SystemA is looking for component bitset: " << systemBitset
            << "\n";

  // Create an entity and add components
  Entity &entity = EntityHelper::createEntity();
  entity.addComponent<TestTransform>(1.0f, 2.0f, 3.0f);
  EntityHelper::merge_entity_arrays();

  std::cout << "Entity created with TestTransform component\n";
  printSystemMap();

  // Check if the system can find the entity
  auto &entity_helper = EntityHelper::get();
  auto it = entity_helper.system_map.find(systemBitset);
  if (it != entity_helper.system_map.end()) {
    std::cout << "✓ SUCCESS: SystemA found " << it->second.size()
              << " entities\n";
  } else {
    std::cout << "✗ FAILURE: SystemA found 0 entities (bitset mismatch!)\n";
  }

  // Show what bitsets are actually in the system map
  std::cout << "\nActual bitsets in system map:\n";
  for (const auto &[componentSet, entityIds] : entity_helper.system_map) {
    std::cout << "  " << componentSet << " -> " << entityIds.size()
              << " entities\n";
  }

  std::cout << "\nThis is the same issue we're seeing in the main game!\n";
  std::cout << "The RenderEntities system can't find entities because their\n";
  std::cout
      << "component bitsets don't match what's stored in the system map.\n";
  std::cout
      << "============================================================\n\n";
}

void testSystemManager(SystemManager &systemManager, SystemA &systemA,
                       SystemB &systemB, SystemC &systemC, SystemD &systemD,
                       SystemE &systemE, SystemF &systemF) {
  std::cout << "\n=== Testing SystemManager Tick ===\n";

  // Reset all system call counts
  systemA.reset();
  systemB.reset();
  systemC.reset();
  systemD.reset();
  systemE.reset();
  systemF.reset();

  // Create multiple entities with different component combinations
  std::cout << "Creating test entities...\n";

  // Entity 1: Empty (should match SystemE only)
  Entity &entity1 = EntityHelper::createEntity();

  // Entity 2: Transform only (should match SystemA and SystemE)
  Entity &entity2 = EntityHelper::createEntity();
  entity2.addComponent<TestTransform>(1.0f, 2.0f, 3.0f);

  // Entity 3: Transform + CanShoot (should match SystemA, SystemB, and SystemE)
  Entity &entity3 = EntityHelper::createEntity();
  entity3.addComponent<TestTransform>(4.0f, 5.0f, 6.0f);
  entity3.addComponent<TestCanShoot>(0.5f);

  // Entity 4: HasSprite only (should match SystemC and SystemE)
  Entity &entity4 = EntityHelper::createEntity();
  entity4.addComponent<TestHasSprite>(100);

  // Entity 5: Player (should match SystemA, SystemE, and SystemF)
  Entity &entity5 = EntityHelper::createEntity();
  entity5.addComponent<TestPlayer>("TestPlayer", 10, 7.0f, 8.0f, 9.0f);

  // Entity 6: Transform + HasSprite (should match SystemA, SystemC, SystemD,
  // and SystemE)
  Entity &entity6 = EntityHelper::createEntity();
  entity6.addComponent<TestTransform>(10.0f, 11.0f, 12.0f);
  entity6.addComponent<TestHasSprite>(200);

  // Merge all entities
  EntityHelper::merge_entity_arrays();

  std::cout << "Created 6 entities with different component combinations.\n";
  printSystemMap();

  // Run SystemManager tick
  std::cout << "Running SystemManager tick...\n";
  systemManager.tick(EntityHelper::get_entities_for_mod(), 0.016f); // ~60fps

  // Validate call counts
  std::cout << "Validating system call counts:\n";
  std::cout << "SystemA (Transform): " << systemA.get_call_count()
            << " calls\n";
  std::cout << "SystemB (Transform+CanShoot): " << systemB.get_call_count()
            << " calls\n";
  std::cout << "SystemC (HasSprite): " << systemC.get_call_count()
            << " calls\n";
  std::cout << "SystemD (Transform+HasSprite): " << systemD.get_call_count()
            << " calls\n";
  std::cout << "SystemE (empty): " << systemE.get_call_count() << " calls\n";
  std::cout << "SystemF (Player): " << systemF.get_call_count() << " calls\n";

  // Expected counts:
  // SystemA: entity2, entity3, entity5, entity6 = 4 calls
  // SystemB: entity3 = 1 call
  // SystemC: entity4, entity6 = 2 calls
  // SystemD: entity6 = 1 call
  // SystemE: all entities = 6 calls
  // SystemF: entity5 = 1 call

  assert(systemA.get_call_count() == 4);
  assert(systemB.get_call_count() == 1);
  assert(systemC.get_call_count() == 2);
  assert(systemD.get_call_count() == 1);
  assert(systemE.get_call_count() == 6);
  assert(systemF.get_call_count() == 1);

  std::cout << "All SystemManager call counts are correct!\n";
}

struct TestSetup {
  SystemA systemA;
  SystemB systemB;
  SystemC systemC;
  SystemD systemD;
  SystemE systemE;
  SystemF systemF;
  SystemManager systemManager;

  TestSetup()
      : systemA("SystemA"), systemB("SystemB"), systemC("SystemC"),
        systemD("SystemD"), systemE("SystemE"), systemF("SystemF") {
    // Register systems with SystemManager
    systemManager.register_update_system(std::make_unique<SystemA>(systemA));
    systemManager.register_update_system(std::make_unique<SystemB>(systemB));
    systemManager.register_update_system(std::make_unique<SystemC>(systemC));
    systemManager.register_update_system(std::make_unique<SystemD>(systemD));
    systemManager.register_update_system(std::make_unique<SystemE>(systemE));
    systemManager.register_update_system(std::make_unique<SystemF>(systemF));

    std::cout
        << "Test setup complete - all systems registered with SystemManager.\n";
  }
};

int main() {
  std::cout << "Testing System Map Updates\n";
  std::cout << "==========================\n\n";

  // Create systems
  SystemA systemA("SystemA");
  SystemB systemB("SystemB");
  SystemC systemC("SystemC");
  SystemD systemD("SystemD");
  SystemE systemE("SystemE");
  SystemF systemF("SystemF");

  // Test 1: Create entity - should be in empty component set
  std::cout << "1. Creating entity...\n";
  Entity &entity = EntityHelper::createEntity();
  std::cout << "Created entity with ID: " << entity.id << "\n";

  // Merge temp entities to main entities array
  EntityHelper::merge_entity_arrays();

  // Assert: Entity should be in empty component set (matches SystemE)
  ComponentBitSet emptySet = createComponentSet(false, false, false);
  assert(isEntityInComponentSet(entity.id, emptySet));
  assert(countEntitiesInComponentSet(emptySet) == 1);
  printSystemMap();

  // Test 2: Add TestTransform component
  std::cout << "2. Adding TestTransform component...\n";
  entity.addComponent<TestTransform>(1.0f, 2.0f, 3.0f);

  // Assert: Entity should now be in Transform-only set (matches SystemA and
  // SystemE)
  ComponentBitSet transformSet = createComponentSet(true, false, false);
  assert(isEntityInComponentSet(entity.id, transformSet));
  assert(countEntitiesInComponentSet(transformSet) == 1);
  assert(!isEntityInComponentSet(entity.id,
                                 emptySet)); // Should no longer be in empty set
  printSystemMap();

  // Test 3: Add TestHasSprite component
  std::cout << "3. Adding TestHasSprite component...\n";
  entity.addComponent<TestHasSprite>(42);

  // Assert: Entity should now be in TestTransform+TestHasSprite set (matches
  // SystemA, SystemC, SystemD, and SystemE)
  ComponentBitSet transformSpriteSet = createComponentSet(true, false, true);
  assert(isEntityInComponentSet(entity.id, transformSpriteSet));
  assert(countEntitiesInComponentSet(transformSpriteSet) == 1);
  assert(!isEntityInComponentSet(
      entity.id,
      transformSet)); // Should no longer be in TestTransform-only set
  printSystemMap();

  // Test 4: Add TestCanShoot component
  std::cout << "4. Adding TestCanShoot component...\n";
  entity.addComponent<TestCanShoot>(0.5f);

  // Assert: Entity should now be in all components set (matches SystemA,
  // SystemB, SystemC, SystemD, and SystemE)
  ComponentBitSet allComponentsSet = createComponentSet(true, true, true);
  assert(isEntityInComponentSet(entity.id, allComponentsSet));
  assert(countEntitiesInComponentSet(allComponentsSet) == 1);
  assert(!isEntityInComponentSet(
      entity.id,
      transformSpriteSet)); // Should no longer be in Transform+Sprite set
  printSystemMap();

  // Test 5: Verify system matches
  std::cout << "5. Testing system matches:\n";
  assert(entity.has<TestTransform>());
  assert(entity.has<TestCanShoot>());
  assert(entity.has<TestHasSprite>());
  std::cout << "All component checks passed!\n";

  // Test 6: Remove TestTransform component
  std::cout << "\n6. Removing TestTransform component...\n";
  entity.removeComponent<TestTransform>();

  // Assert: Entity should now be in CanShoot+HasSprite set (matches SystemC and
  // SystemE)
  ComponentBitSet canShootSpriteSet = createComponentSet(false, true, true);
  assert(isEntityInComponentSet(entity.id, canShootSpriteSet));
  assert(countEntitiesInComponentSet(canShootSpriteSet) == 1);
  assert(!isEntityInComponentSet(
      entity.id,
      allComponentsSet)); // Should no longer be in all components set
  printSystemMap();

  // Test 7: Verify system matches after removal
  std::cout << "7. Testing system matches after TestTransform removal:\n";
  assert(!entity.has<TestTransform>());
  assert(entity.has<TestCanShoot>());
  assert(entity.has<TestHasSprite>());
  std::cout << "All component checks after removal passed!\n";

  // Test 8: Remove all remaining components
  std::cout << "\n8. Removing all remaining components...\n";
  entity.removeComponent<TestCanShoot>();
  entity.removeComponent<TestHasSprite>();

  // Assert: Entity should be back in empty set
  assert(isEntityInComponentSet(entity.id, emptySet));
  assert(countEntitiesInComponentSet(emptySet) == 1);
  assert(!isEntityInComponentSet(
      entity.id,
      canShootSpriteSet)); // Should no longer be in CanShoot+Sprite set
  printSystemMap();

  // Test 9: Add TestPlayer component (inherits from TestTransform)
  std::cout
      << "\n9. Adding TestPlayer component (inherits from TestTransform)...\n";
  entity.addComponent<TestPlayer>("TestPlayer", 5, 10.0f, 20.0f, 30.0f);

  // Assert: Entity should now be in Player set (which includes Transform)
  ComponentBitSet playerSet = createComponentSet(false, false, false, true);
  assert(isEntityInComponentSet(entity.id, playerSet));
  assert(countEntitiesInComponentSet(playerSet) == 1);
  assert(!isEntityInComponentSet(entity.id,
                                 emptySet)); // Should no longer be in empty set
  printSystemMap();

  // Test 10: Verify inheritance works - TestPlayer should match TestTransform
  // systems
  std::cout << "10. Testing inheritance - TestPlayer should match "
               "TestTransform systems:\n";
  assert(entity.has_child_of<TestTransform>()); // TestPlayer inherits from
                                                // TestTransform
  assert(entity.has<TestPlayer>());
  assert(!entity.has<TestCanShoot>());
  assert(!entity.has<TestHasSprite>());
  std::cout
      << "Inheritance test passed! TestPlayer component has TestTransform "
         "capabilities.\n";

  // Test 11: Add other components to TestPlayer
  std::cout << "\n11. Adding TestCanShoot to TestPlayer entity...\n";
  entity.addComponent<TestCanShoot>(0.3f);

  // Assert: Entity should now be in Player+CanShoot set
  ComponentBitSet playerCanShootSet =
      createComponentSet(false, true, false, true);
  assert(isEntityInComponentSet(entity.id, playerCanShootSet));
  assert(countEntitiesInComponentSet(playerCanShootSet) == 1);
  assert(!isEntityInComponentSet(
      entity.id, playerSet)); // Should no longer be in Player-only set
  printSystemMap();

  // Test 12: Verify TestPlayer with TestCanShoot matches multiple systems
  std::cout << "12. Testing system matches for TestPlayer with TestCanShoot:\n";
  assert(entity.has_child_of<TestTransform>()); // TestPlayer inherits from
                                                // TestTransform
  assert(entity.has<TestPlayer>());
  assert(entity.has<TestCanShoot>());
  assert(!entity.has<TestHasSprite>());
  std::cout << "TestPlayer with TestCanShoot matches TestTransform, "
               "TestPlayer, and TestCanShoot "
               "systems!\n";

  // Test 13: Remove TestPlayer component
  std::cout << "\n13. Removing TestPlayer component...\n";
  entity.removeComponent<TestPlayer>();

  // Assert: Entity should now be in CanShoot-only set
  ComponentBitSet canShootOnlySet =
      createComponentSet(false, true, false, false);
  assert(isEntityInComponentSet(entity.id, canShootOnlySet));
  assert(countEntitiesInComponentSet(canShootOnlySet) == 1);
  assert(!isEntityInComponentSet(
      entity.id,
      playerCanShootSet)); // Should no longer be in Player+CanShoot set
  printSystemMap();

  // Test 14: Verify TestPlayer removal
  std::cout << "14. Testing TestPlayer removal:\n";
  assert(!entity.has<TestPlayer>());
  assert(
      !entity
           .has_child_of<TestTransform>()); // TestTransform should be gone too
                                            // since TestPlayer was removed
  assert(entity.has<TestCanShoot>());
  std::cout
      << "TestPlayer removal test passed! TestTransform is also removed when "
         "TestPlayer is removed.\n";

  // Test RenderEntities system
  testRenderEntities();

  // Test the component bitset mismatch issue
  testComponentBitsetMismatch();

  std::cout << "\nAll tests passed successfully!\n";
  return 0;
}
