#pragma once

#include <unordered_map>
#include <vector>

#include "base_component.h"
#include "entity.h"

namespace afterhours {

// Forward declaration
using EntityID = int;

// Type-erased base class for component storages
// Defined here so ComponentStorage can inherit from it
struct ComponentStorageBase {
  virtual ~ComponentStorageBase() = default;
  virtual void remove_component(EntityID eid) = 0;
  virtual bool has_component(EntityID eid) const = 0;
  virtual size_t size() const = 0;
  virtual void clear() = 0;
};

// Component storage for SOA architecture
// Stores components of a specific type in dense arrays
template <typename Component>
struct ComponentStorage : public ComponentStorageBase {
  static_assert(std::is_base_of<BaseComponent, Component>::value,
                "Component must inherit from BaseComponent");
  
  // Dense array of components
  std::vector<Component> components;
  
  // Parallel array of entity IDs (components[i] belongs to entity_ids[i])
  std::vector<EntityID> entity_ids;
  
  // Fast lookup: EntityID -> index in components array
  std::unordered_map<EntityID, size_t> entity_to_index;
  
  ComponentStorage() {
    components.reserve(1000);
    entity_ids.reserve(1000);
  }
  
  // Add component for an entity
  template <typename... Args>
  Component &add_component(EntityID eid, Args &&...args) {
    auto it = entity_to_index.find(eid);
    if (it != entity_to_index.end()) {
      // Component already exists, return reference to existing
      return components[it->second];
    }
    
    // Create new component
    size_t idx = components.size();
    components.emplace_back(std::forward<Args>(args)...);
    entity_ids.push_back(eid);
    entity_to_index[eid] = idx;
    
    return components[idx];
  }
  
  // Get component for an entity (returns nullptr if not found)
  Component *get_component(EntityID eid) {
    auto it = entity_to_index.find(eid);
    if (it != entity_to_index.end()) {
      return &components[it->second];
    }
    return nullptr;
  }
  
  // Get component for an entity (const version)
  const Component *get_component(EntityID eid) const {
    auto it = entity_to_index.find(eid);
    if (it != entity_to_index.end()) {
      return &components[it->second];
    }
    return nullptr;
  }
  
  // Check if entity has this component
  bool has_component(EntityID eid) const override {
    return entity_to_index.contains(eid);
  }
  
  // Remove component for an entity
  void remove_component(EntityID eid) override {
    auto it = entity_to_index.find(eid);
    if (it == entity_to_index.end()) {
      return; // Component doesn't exist
    }
    
    size_t idx = it->second;
    size_t last_idx = components.size() - 1;
    
    if (idx != last_idx) {
      // Move last component to this position (swap and pop)
      EntityID last_eid = entity_ids[last_idx];
      components[idx] = std::move(components[last_idx]);
      entity_ids[idx] = last_eid;
      entity_to_index[last_eid] = idx;
    }
    
    // Remove last element
    components.pop_back();
    entity_ids.pop_back();
    entity_to_index.erase(eid);
  }
  
  // Get all entity IDs that have this component
  std::vector<EntityID> get_all_entity_ids() const {
    std::vector<EntityID> result;
    result.reserve(entity_ids.size());
    for (EntityID eid : entity_ids) {
      result.push_back(eid);
    }
    return result;
  }
  
  // Get count of entities with this component
  size_t size() const override {
    return components.size();
  }
  
  // Check if empty
  bool empty() const {
    return components.empty();
  }
  
  // Clear all components
  void clear() override {
    components.clear();
    entity_ids.clear();
    entity_to_index.clear();
  }
  
  // Iterate over all components (for systems that need direct access)
  template <typename Func>
  void for_each(Func &&func) {
    for (size_t i = 0; i < components.size(); ++i) {
      func(entity_ids[i], components[i]);
    }
  }
  
  template <typename Func>
  void for_each(Func &&func) const {
    for (size_t i = 0; i < components.size(); ++i) {
      func(entity_ids[i], components[i]);
    }
  }
};

} // namespace afterhours

