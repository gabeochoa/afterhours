#pragma once

#include <bitset>
#include <set>
#include <unordered_map>
#include <vector>

#include "base_component.h"
#include "entity.h"

namespace afterhours {

// Component fingerprint storage for SOA architecture
// Stores ComponentBitSet in dense arrays for fast filtering
struct ComponentFingerprintStorage {
  // Dense array of component fingerprints
  std::vector<ComponentBitSet> fingerprints;
  
  // Parallel array of entity IDs (fingerprints[i] belongs to entity_ids[i])
  std::vector<EntityID> entity_ids;
  
  // Fast lookup: EntityID -> index in fingerprints array
  std::unordered_map<EntityID, size_t> entity_to_index;
  
  // Track which EntityIDs are marked for cleanup
  std::set<EntityID> cleanup_marked;
  
  ComponentFingerprintStorage() {
    fingerprints.reserve(1000);
    entity_ids.reserve(1000);
  }
  
  // Add fingerprint for an entity
  void add_entity(EntityID eid, const ComponentBitSet &fingerprint) {
    if (entity_to_index.contains(eid)) {
      // Entity already exists, update fingerprint
      size_t idx = entity_to_index[eid];
      fingerprints[idx] = fingerprint;
      return;
    }
    
    size_t idx = fingerprints.size();
    fingerprints.push_back(fingerprint);
    entity_ids.push_back(eid);
    entity_to_index[eid] = idx;
  }
  
  // Update fingerprint for an entity
  void update_fingerprint(EntityID eid, const ComponentBitSet &fingerprint) {
    auto it = entity_to_index.find(eid);
    if (it != entity_to_index.end()) {
      fingerprints[it->second] = fingerprint;
    }
  }
  
  // Remove entity (mark for cleanup, actual removal happens in cleanup())
  void mark_for_cleanup(EntityID eid) {
    cleanup_marked.insert(eid);
  }
  
  // Get fingerprint for an entity
  ComponentBitSet get_fingerprint(EntityID eid) const {
    auto it = entity_to_index.find(eid);
    if (it != entity_to_index.end()) {
      return fingerprints[it->second];
    }
    return ComponentBitSet{}; // Empty fingerprint if not found
  }
  
  // Check if entity exists
  bool has_entity(EntityID eid) const {
    return entity_to_index.contains(eid);
  }
  
  // Get all entity IDs
  std::vector<EntityID> get_all_entity_ids() const {
    std::vector<EntityID> result;
    result.reserve(entity_ids.size());
    for (EntityID eid : entity_ids) {
      if (!cleanup_marked.contains(eid)) {
        result.push_back(eid);
      }
    }
    return result;
  }
  
  // Cleanup: Remove entities marked for cleanup
  void cleanup() {
    if (cleanup_marked.empty()) {
      return;
    }
    
    // Build new arrays without cleanup-marked entities
    std::vector<ComponentBitSet> new_fingerprints;
    std::vector<EntityID> new_entity_ids;
    std::unordered_map<EntityID, size_t> new_entity_to_index;
    
    new_fingerprints.reserve(fingerprints.size());
    new_entity_ids.reserve(entity_ids.size());
    
    for (size_t i = 0; i < fingerprints.size(); ++i) {
      EntityID eid = entity_ids[i];
      if (!cleanup_marked.contains(eid)) {
        size_t new_idx = new_fingerprints.size();
        new_fingerprints.push_back(fingerprints[i]);
        new_entity_ids.push_back(eid);
        new_entity_to_index[eid] = new_idx;
      }
    }
    
    // Replace old arrays
    fingerprints = std::move(new_fingerprints);
    entity_ids = std::move(new_entity_ids);
    entity_to_index = std::move(new_entity_to_index);
    cleanup_marked.clear();
  }
  
  // Get count of active entities (not marked for cleanup)
  size_t size() const {
    return fingerprints.size() - cleanup_marked.size();
  }
  
  // Check if empty
  bool empty() const {
    return fingerprints.empty() || fingerprints.size() == cleanup_marked.size();
  }
  
  // Clear all entities
  void clear() {
    fingerprints.clear();
    entity_ids.clear();
    entity_to_index.clear();
    cleanup_marked.clear();
  }
};

} // namespace afterhours

