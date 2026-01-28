#pragma once

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include "../debug_allocator.h"
#include "../logging.h"
#include "../type_name.h"
#include "base_component.h"
#include "entity.h"
#include "entity_handle.h"

namespace afterhours {

// Type aliases - these match the ones in entity_helper.h
// Defined here to avoid circular dependency
using EntityType = std::shared_ptr<Entity>;

#ifdef AFTER_HOURS_ENTITY_ALLOC_DEBUG
using EntityAllocator = developer::DebugAllocator<EntityType>;
#else
using EntityAllocator = std::allocator<EntityType>;
#endif

using Entities = std::vector<EntityType, EntityAllocator>;
using RefEntities = std::vector<RefEntity>;

// EntityCollection: Storage container for entities, handles, and related data.
// Supports multiple independent collections for multi-threaded scenarios.
struct EntityCollection {
  Entities entities_DO_NOT_USE;
  Entities temp_entities;
  std::set<int> permanant_ids;
  std::map<ComponentID, Entity *> singletonMap;

  // Handle store:
  // - stable slot table + generation counters
  // - id->slot mapping for O(1) EntityID resolution
  struct Slot {
    EntityType ent;
    EntityHandle::Slot gen = 1;
  };

  std::vector<Slot> slots;
  std::vector<EntityHandle::Slot> free_slots;
  std::vector<EntityHandle::Slot> id_to_slot;

  struct CreationOptions {
    bool is_permanent;
  };

  // Bump a slot generation counter so old handles become stale.
  // Returns a non-zero generation (wraparound skips 0).
  static EntityHandle::Slot bump_gen(EntityHandle::Slot gen) {
    // Unsigned wraparound is well-defined; if we wrapped to 0, bump to 1.
    const EntityHandle::Slot next = gen + 1;
    return next + static_cast<EntityHandle::Slot>(next == 0);
  }

  void reserve_temp_space() { temp_entities.reserve(sizeof(EntityType) * 100); }

  Entities &get_temp() { return temp_entities; }
  const Entities &get_temp() const { return temp_entities; }

  Entities &get_entities_for_mod() { return entities_DO_NOT_USE; }
  const Entities &get_entities() const { return entities_DO_NOT_USE; }

  // Allocate a slot index for a (merged) entity.
  // - Reuses a free slot if available
  // - Otherwise grows the slot table
  EntityHandle::Slot alloc_slot_index() {
    if (!free_slots.empty()) {
      const EntityHandle::Slot slot = free_slots.back();
      free_slots.pop_back();
      return slot;
    }
    slots.push_back(Slot{});
    return static_cast<EntityHandle::Slot>(slots.size() - 1);
  }

  // Ensure `id_to_slot[id]` is in-bounds.
  // New entries are initialized to INVALID_SLOT.
  void ensure_id_mapping_size(const EntityID id) {
    if (id < 0)
      return;
    const auto need = static_cast<std::size_t>(id) + 1;
    if (id_to_slot.size() < need) {
      id_to_slot.resize(need, EntityHandle::INVALID_SLOT);
    }
  }

  // Assign a stable slot to an entity (if it doesn't already have one).
  // Also updates the O(1) `EntityID -> slot` mapping.
  void assign_slot_to_entity(const EntityType &sp) {
    if (!sp)
      return;
    if (sp->ah_slot_index != EntityHandle::INVALID_SLOT) {
      // already assigned (should be rare in default config)
      ensure_id_mapping_size(sp->id);
      if (sp->id >= 0)
        id_to_slot[static_cast<std::size_t>(sp->id)] = sp->ah_slot_index;
      return;
    }

    const EntityHandle::Slot slot = alloc_slot_index();
    if (slot >= slots.size()) {
      log_error("alloc_slot_index returned out-of-range slot {}", slot);
      return;
    }
    slots[slot].ent = sp;
    sp->ah_slot_index = slot;

    ensure_id_mapping_size(sp->id);
    if (sp->id >= 0)
      id_to_slot[static_cast<std::size_t>(sp->id)] = slot;
  }

  // Invalidate an entity's slot and ID mapping (if any).
  // - Clears id_to_slot[entity.id]
  // - Clears slots[slot].ent
  // - Bumps slots[slot].gen (stales old handles)
  // - Adds slot back to the free list
  void invalidate_entity_slot_if_any(const EntityType &sp) {
    if (!sp)
      return;
    const EntityID id = sp->id;
    const EntityHandle::Slot slot = sp->ah_slot_index;
    sp->ah_slot_index = EntityHandle::INVALID_SLOT;

    if (id >= 0 && static_cast<std::size_t>(id) < id_to_slot.size()) {
      if (id_to_slot[static_cast<std::size_t>(id)] == slot) {
        id_to_slot[static_cast<std::size_t>(id)] = EntityHandle::INVALID_SLOT;
      }
    }

    if (slot == EntityHandle::INVALID_SLOT)
      return;
    if (slot >= slots.size()) {
      log_error("invalidate_entity_slot_if_any: out-of-range slot {}", slot);
      return;
    }

    Slot &s = slots[slot];
    if (s.ent) {
      s.ent.reset();
    }
    s.gen = bump_gen(s.gen);
    free_slots.push_back(slot);
  }

  // Return a stable handle for a currently-merged entity.
  // Returns invalid if the entity has no slot yet (temp pre-merge) or if the
  // slot doesn't currently point at this entity.
  EntityHandle handle_for(const Entity &e) const {
    const EntityHandle::Slot slot = e.ah_slot_index;
    if (slot == EntityHandle::INVALID_SLOT)
      return EntityHandle::invalid();

    if (slot >= slots.size())
      return EntityHandle::invalid();
    const Slot &s = slots[slot];
    if (!s.ent || s.ent.get() != &e)
      return EntityHandle::invalid();
    return {slot, s.gen};
  }

  // Resolve a handle into an entity reference (if still alive).
  // Returns empty if:
  // - handle is invalid
  // - slot is out of range
  // - generation mismatch (stale handle)
  // - slot is empty (entity deleted)
  OptEntity resolve(const EntityHandle h) const {
    if (h.is_invalid())
      return {};
    if (h.slot >= slots.size())
      return {};
    const Slot &s = slots[h.slot];
    if (s.gen != h.gen)
      return {};
    if (!s.ent)
      return {};
    return *s.ent;
  }

  Entity &createEntity() {
    return createEntityWithOptions({.is_permanent = false});
  }

  Entity &createPermanentEntity() {
    return createEntityWithOptions({.is_permanent = true});
  }

  Entity &createEntityWithOptions(const CreationOptions &options) {
    if (temp_entities.capacity() == 0) [[unlikely]]
      reserve_temp_space();

    std::shared_ptr<Entity> e(new Entity());
    temp_entities.push_back(e);

    if (options.is_permanent) {
      permanant_ids.insert(e->id);
    }

    return *e;
  }

  void merge_entity_arrays() {
    if (temp_entities.empty())
      return;

    for (const auto &entity : temp_entities) {
      if (!entity)
        continue;
      if (entity->cleanup)
        continue;
      entities_DO_NOT_USE.push_back(entity);
      assign_slot_to_entity(entity);
    }
    temp_entities.clear();
  }

  template <typename Component> void registerSingleton(Entity &ent) {
    const ComponentID id = components::get_type_id<Component>();

    if (singletonMap.contains(id)) {
      log_error("Already had registered singleton {}", type_name<Component>());
    }

    singletonMap.emplace(id, &ent);
    log_info("Registered singleton {} for {} ({})", ent.id,
             type_name<Component>(), id);
  }

  template <typename Component> RefEntity get_singleton() const {
    const ComponentID id = components::get_type_id<Component>();
    if (!singletonMap.contains(id)) {
      log_warn("Singleton map is missing value for component {} ({}). Did you "
               "register this component previously?",
               id, type_name<Component>());
      // Return a reference to a static dummy entity to avoid crash
      // This should never happen in proper usage, but prevents segfault
      static Entity dummy_entity;
      return dummy_entity;
    }
    auto *entity_ptr = singletonMap.at(id);
    if (!entity_ptr) {
      log_error("Singleton map contains null pointer for component {} ({})", id,
                type_name<Component>());
      static Entity dummy_entity;
      return dummy_entity;
    }
    return *entity_ptr;
  }

  template <typename Component> Component *get_singleton_cmp() const {
    Entity &ent = get_singleton<Component>();
    return &(ent.get<Component>());
  }

  template <typename Component> const Component *get_singleton_cmp_const() const {
    Entity &ent = get_singleton<Component>();
    return &(ent.get<Component>());
  }

  template <typename Component> bool has_singleton() const {
    const ComponentID id = components::get_type_id<Component>();
    return singletonMap.contains(id);
  }

  void markIDForCleanup(const int e_id) {
    const auto &entities = get_entities();
    auto it = entities.begin();
    while (it != entities.end()) {
      if ((*it)->id == e_id) {
        (*it)->cleanup = true;
        break;
      }
      it++;
    }
  }

  void cleanup() {
    merge_entity_arrays();
    Entities &entities = get_entities_for_mod();

    std::size_t i = 0;
    while (i < entities.size()) {
      const auto &sp = entities[i];
      if (sp && !sp->cleanup) {
        ++i;
        continue;
      }
      // Remove any singleton registrations pointing at this entity.
      // (We can't leave dangling Entity* in singletonMap.)
      if (sp) {
        Entity *removed = sp.get();
        for (auto it = singletonMap.begin(); it != singletonMap.end();) {
          if (it->second == removed) {
            it = singletonMap.erase(it);
          } else {
            ++it;
          }
        }
      }
      // invalidate removed entity slot/id mapping
      invalidate_entity_slot_if_any(entities[i]);
      if (i != entities.size() - 1) {
        std::swap(entities[i], entities.back());
      }
      entities.pop_back();
    }
  }

  void delete_all_entities_NO_REALLY_I_MEAN_ALL() {
    Entities &entities = get_entities_for_mod();

    // Invalidate slots for all entities we currently know about.
    for (auto &sp : entities) {
      invalidate_entity_slot_if_any(sp);
    }
    for (auto &sp : temp_entities) {
      invalidate_entity_slot_if_any(sp);
    }

    entities.clear();
    temp_entities.clear();
    permanant_ids.clear();
    singletonMap.clear();
  }

  void delete_all_entities(const bool include_permanent) {
    merge_entity_arrays();

    if (include_permanent) {
      delete_all_entities_NO_REALLY_I_MEAN_ALL();
      return;
    }

    Entities &entities = get_entities_for_mod();
    std::size_t i = 0;
    while (i < entities.size()) {
      const auto &sp = entities[i];
      const bool keep = sp && permanant_ids.contains(sp->id);
      if (keep) {
        ++i;
        continue;
      }

      // Remove singleton registrations pointing to the entity we're
      // deleting.
      if (sp) {
        Entity *removed = sp.get();
        for (auto it = singletonMap.begin(); it != singletonMap.end();) {
          if (it->second == removed) {
            it = singletonMap.erase(it);
          } else {
            ++it;
          }
        }
      }

      // Invalidate slot/id mapping so lookups and handles don't retain
      // references to removed entities.
      invalidate_entity_slot_if_any(entities[i]);

      if (i != entities.size() - 1)
        std::swap(entities[i], entities.back());
      entities.pop_back();
    }
  }

  // Rebuild the handle store (slots/free list/id mapping) from the current
  // `entities_DO_NOT_USE` contents. This is intended for integration points
  // that bulk-replace the entity list (e.g., loading a snapshot).
  //
  // Note: this does NOT preserve handle values across rebuilds; it creates a
  // fresh slot table consistent with the current entities.
  void rebuild_handle_store_from_entities() {
    slots.clear();
    free_slots.clear();
    id_to_slot.clear();

    // Ensure entities don't think they already have a slot.
    for (auto &sp : entities_DO_NOT_USE) {
      if (!sp)
        continue;
      sp->ah_slot_index = EntityHandle::INVALID_SLOT;
    }

    for (auto &sp : entities_DO_NOT_USE) {
      if (!sp)
        continue;
      if (sp->cleanup)
        continue;
      assign_slot_to_entity(sp);
    }
  }

  // Replace the entire entity list with a new one and rebuild handle/id
  // indices accordingly.
  void replace_all_entities(Entities new_entities) {
    // Clear all runtime-only state.
    entities_DO_NOT_USE.clear();
    temp_entities.clear();
    permanant_ids.clear();
    singletonMap.clear();

    // Replace and rebuild indices.
    entities_DO_NOT_USE = std::move(new_entities);
    rebuild_handle_store_from_entities();
  }

  OptEntity getEntityForID(const EntityID id) const {
    if (id == -1)
      return {};

    if (id >= 0) {
      const std::size_t idx = static_cast<std::size_t>(id);
      if (idx < id_to_slot.size()) {
        const EntityHandle::Slot slot = id_to_slot[idx];
        if (slot != EntityHandle::INVALID_SLOT && slot < slots.size()) {
          if (const auto &sp = slots[slot].ent; sp && sp->id == id) {
            return *sp;
          }
        }
      }
    }

// In debug builds we can do an expensive scan to help catch missing mapping
// updates during development/integration. Release builds should never scan.
#if defined(AFTER_HOURS_DEBUG)
    log_warn("getEntityForID fallback scan for id={} (id_to_slot.size={}, "
             "slots.size={})",
             id, id_to_slot.size(), slots.size());
    for (const auto &e : get_entities()) {
      if (!e)
        continue;
      if (e->id == id)
        return *e;
    }
#endif
    return {};
  }

  Entity &getEntityForIDEnforce(const EntityID id) const {
    auto opt_ent = getEntityForID(id);
    return opt_ent.asE();
  }
};

} // namespace afterhours
