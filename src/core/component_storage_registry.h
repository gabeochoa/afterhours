#pragma once

#include <memory>
#include <unordered_map>

#include "base_component.h"
#include "component_storage.h"

namespace afterhours {

// Type-erased base class for component storages
struct ComponentStorageBase {
  virtual ~ComponentStorageBase() = default;
  virtual void remove_component(EntityID eid) = 0;
  virtual bool has_component(EntityID eid) const = 0;
  virtual size_t size() const = 0;
  virtual void clear() = 0;
};

// Registry for component storages (one storage per component type)
struct ComponentStorageRegistry {
  // Type-erased storage map: ComponentID -> ComponentStorageBase*
  std::unordered_map<ComponentID, std::unique_ptr<ComponentStorageBase>> storages;
  
  // Get or create ComponentStorage for a specific component type
  template <typename Component>
  ComponentStorage<Component> &get_or_create_storage() {
    ComponentID id = components::get_type_id<Component>();
    
    auto it = storages.find(id);
    if (it != storages.end()) {
      // Storage exists, cast and return
      auto *storage = dynamic_cast<ComponentStorage<Component> *>(it->second.get());
      if (storage) {
        return *storage;
      }
      // Wrong type stored, replace it
      storages.erase(it);
    }
    
    // Create new storage
    auto storage = std::make_unique<ComponentStorage<Component>>();
    ComponentStorage<Component> *storage_ptr = storage.get();
    storages[id] = std::move(storage);
    
    return *storage_ptr;
  }
  
  // Get ComponentStorage (must exist)
  template <typename Component>
  ComponentStorage<Component> &get_storage() {
    ComponentID id = components::get_type_id<Component>();
    auto it = storages.find(id);
    if (it == storages.end()) {
      // Storage doesn't exist, create it
      return get_or_create_storage<Component>();
    }
    
    auto *storage = dynamic_cast<ComponentStorage<Component> *>(it->second.get());
    if (!storage) {
      // Wrong type, recreate
      storages.erase(it);
      return get_or_create_storage<Component>();
    }
    
    return *storage;
  }
  
  // Check if storage exists
  template <typename Component>
  bool has_storage() const {
    ComponentID id = components::get_type_id<Component>();
    return storages.contains(id);
  }
  
  // Remove component from all storages
  void remove_entity_from_all(EntityID eid) {
    for (auto &[id, storage] : storages) {
      storage->remove_component(eid);
    }
  }
  
  // Mark entity for cleanup in all storages (if they support it)
  void mark_entity_for_cleanup(EntityID eid) {
    // ComponentStorage handles cleanup via remove_component, so this is a no-op
    // But we keep it for API consistency with fingerprint storage
  }
  
  // Cleanup marked entities (remove them from all storages)
  void cleanup() {
    // ComponentStorage handles cleanup via remove_component during normal operations
    // This is a no-op but kept for API consistency
  }
  
  // Clear all storages
  void clear_all() {
    storages.clear();
  }
  
  // Get total number of component storages
  size_t size() const {
    return storages.size();
  }
};

} // namespace afterhours

