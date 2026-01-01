#pragma once

#include <algorithm>
#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include "../debug_allocator.h"
#include "component_store.h"
#include "entity.h"

namespace afterhours {
using EntityType = std::shared_ptr<Entity>;

#ifdef AFTER_HOURS_ENTITY_ALLOC_DEBUG
using EntityAllocator = developer::DebugAllocator<EntityType>;
#else
using EntityAllocator = std::allocator<EntityType>;
#endif

using Entities = std::vector<EntityType, EntityAllocator>;

using RefEntities = std::vector<RefEntity>;

struct EntityHelper;
// Legacy/global accessor (defined in ecs_world.h).
EntityHelper &global_entity_helper();

struct EntityHelper {
  // Legacy/global access path (process-default world).
  // Multi-world callers should prefer owning an EntityHelper per world.
  static EntityHelper &get() { return global_entity_helper(); }

  // Bind this helper to a specific world instance (component storage + id gen).
  void bind(ComponentStore &store, std::atomic_int &entity_id_gen) {
    store_ = &store;
    entity_id_gen_ = &entity_id_gen;
  }

  [[nodiscard]] ComponentStore &component_store() {
    return store_ ? *store_ : ComponentStore::get();
  }
  [[nodiscard]] const ComponentStore &component_store() const {
    return store_ ? *store_ : ComponentStore::get();
  }

  [[nodiscard]] EntityID next_entity_id() {
    if (!entity_id_gen_) {
      return static_cast<EntityID>(ENTITY_ID_GEN.fetch_add(1));
    }
    return static_cast<EntityID>(entity_id_gen_->fetch_add(1));
  }

  Entities entities_DO_NOT_USE;
  Entities temp_entities;
  std::set<int> permanant_ids;
  std::map<ComponentID, Entity *> singletonMap;

  // Handle store:
  // - stable slot table + generation counters
  // - id->slot mapping for O(1) EntityID resolution
  struct Slot {
    EntityType ent{};
    EntityHandle::Slot gen = 1;
  };

  std::vector<Slot> slots;
  std::vector<EntityHandle::Slot> free_slots;
  std::vector<EntityHandle::Slot> id_to_slot;

  struct CreationOptions {
    bool is_permanent;
  };

  enum ForEachFlow {
    NormalFlow = 0,
    Continue = 1,
    Break = 2,
  };

  static void reserve_temp_space() {
    EntityHelper::get().temp_entities.reserve(sizeof(EntityType) * 100);
  }

  static Entities &get_temp() { return EntityHelper::get().temp_entities; }

  static Entities &get_entities_for_mod() {
    return EntityHelper::get().entities_DO_NOT_USE;
  }
  static const Entities &get_entities() { return get_entities_for_mod(); }

  // Bump a slot generation counter so old handles become stale.
  // Returns a non-zero generation (wraparound skips 0).
  static EntityHandle::Slot bump_gen(EntityHandle::Slot gen) {
    // Unsigned wraparound is well-defined; if we wrapped to 0, bump to 1.
    const EntityHandle::Slot next = gen + 1;
    return next + static_cast<EntityHandle::Slot>(next == 0);
  }

  // Allocate a slot index for a (merged) entity.
  // - Reuses a free slot if available
  // - Otherwise grows the slot table
  static EntityHandle::Slot alloc_slot_index() {
    auto &self = EntityHelper::get();
    if (!self.free_slots.empty()) {
      const EntityHandle::Slot slot = self.free_slots.back();
      self.free_slots.pop_back();
      return slot;
    }
    self.slots.push_back(Slot{});
    return static_cast<EntityHandle::Slot>(self.slots.size() - 1);
  }

  // Ensure `id_to_slot[id]` is in-bounds.
  // New entries are initialized to INVALID_SLOT.
  static void ensure_id_mapping_size(const EntityID id) {
    if (id < 0)
      return;
    auto &self = EntityHelper::get();
    const auto need = static_cast<std::size_t>(id) + 1;
    if (self.id_to_slot.size() < need) {
      self.id_to_slot.resize(need, EntityHandle::INVALID_SLOT);
    }
  }

  // Assign a stable slot to an entity (if it doesn't already have one).
  // Also updates the O(1) `EntityID -> slot` mapping.
  static void assign_slot_to_entity(const EntityType &sp) {
    if (!sp)
      return;
    auto &self = EntityHelper::get();
    if (sp->ah_slot_index != EntityHandle::INVALID_SLOT) {
      // already assigned (should be rare in default config)
      ensure_id_mapping_size(sp->id);
      if (sp->id >= 0)
        self.id_to_slot[static_cast<std::size_t>(sp->id)] = sp->ah_slot_index;
      return;
    }

    const EntityHandle::Slot slot = alloc_slot_index();
    if (slot >= self.slots.size()) {
      log_error("alloc_slot_index returned out-of-range slot {}", slot);
      return;
    }
    self.slots[slot].ent = sp;
    sp->ah_slot_index = slot;

    ensure_id_mapping_size(sp->id);
    if (sp->id >= 0)
      self.id_to_slot[static_cast<std::size_t>(sp->id)] = slot;
  }

  // Invalidate an entity's slot and ID mapping (if any).
  // - Clears id_to_slot[entity.id]
  // - Clears slots[slot].ent
  // - Bumps slots[slot].gen (stales old handles)
  // - Adds slot back to the free list
  static void invalidate_entity_slot_if_any(const EntityType &sp) {
    if (!sp)
      return;
    auto &self = EntityHelper::get();
    const EntityID id = sp->id;
    const EntityHandle::Slot slot = sp->ah_slot_index;
    sp->ah_slot_index = EntityHandle::INVALID_SLOT;

    if (id >= 0 && static_cast<std::size_t>(id) < self.id_to_slot.size()) {
      if (self.id_to_slot[static_cast<std::size_t>(id)] == slot) {
        self.id_to_slot[static_cast<std::size_t>(id)] = EntityHandle::INVALID_SLOT;
      }
    }

    if (slot == EntityHandle::INVALID_SLOT)
      return;
    if (slot >= self.slots.size()) {
      log_error("invalidate_entity_slot_if_any: out-of-range slot {}", slot);
      return;
    }

    Slot &s = self.slots[slot];
    if (s.ent) {
      s.ent.reset();
    }
    s.gen = bump_gen(s.gen);
    self.free_slots.push_back(slot);
  }

  // Public handle helpers (additive API)
  // Return a stable handle for a currently-merged entity.
  // Returns invalid if the entity has no slot yet (temp pre-merge) or if the
  // slot doesn't currently point at this entity.
  static EntityHandle handle_for(const Entity &e) {
    const EntityHandle::Slot slot = e.ah_slot_index;
    if (slot == EntityHandle::INVALID_SLOT)
      return EntityHandle::invalid();

    auto &self = EntityHelper::get();
    if (slot >= self.slots.size())
      return EntityHandle::invalid();
    const Slot &s = self.slots[slot];
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
  static OptEntity resolve(const EntityHandle h) {
    if (h.is_invalid())
      return {};
    auto &self = EntityHelper::get();
    if (h.slot >= self.slots.size())
      return {};
    Slot &s = self.slots[h.slot];
    if (s.gen != h.gen)
      return {};
    if (!s.ent)
      return {};
    return *s.ent;
  }

  static RefEntities get_ref_entities() {
    RefEntities matching;
    for (const auto &e : EntityHelper::get_entities()) {
      if (!e)
        continue;
      matching.push_back(*e);
    }
    return matching;
  }

  static Entity &createEntity() {
    return createEntityWithOptions({.is_permanent = false});
  }

  static Entity &createPermanentEntity() {
    return createEntityWithOptions({.is_permanent = true});
  }

  static Entity &createEntityWithOptions(const CreationOptions &options) {
    if (get_temp().capacity() == 0) [[unlikely]]
      reserve_temp_space();

    auto &self = EntityHelper::get();
    std::shared_ptr<Entity> e(
        new Entity(ComponentStore::get(), self.next_entity_id()));
    get_temp().push_back(e);

    if (options.is_permanent) {
      self.permanant_ids.insert(e->id);
    }

    return *e;
  }

  static void merge_entity_arrays() {
    if (get_temp().empty())
      return;

    for (const auto &entity : get_temp()) {
      if (!entity)
        continue;
      if (entity->cleanup)
        continue;
      get_entities_for_mod().push_back(entity);
      assign_slot_to_entity(entity);
    }
    get_temp().clear();
  }

  template <typename Component> static void registerSingleton(Entity &ent) {
    const ComponentID id = components::get_type_id<Component>();

    if (EntityHelper::get().singletonMap.contains(id)) {
      log_error("Already had registered singleton {}", type_name<Component>());
    }

    EntityHelper::get().singletonMap.emplace(id, &ent);
    log_info("Registered singleton {} for {} ({})", ent.id,
             type_name<Component>(), id);
  }

  template <typename Component> static RefEntity get_singleton() {
    const ComponentID id = components::get_type_id<Component>();
    auto &singleton_map = EntityHelper::get().singletonMap;
    if (!singleton_map.contains(id)) {
      log_warn("Singleton map is missing value for component {} ({}). Did you "
               "register this component previously?",
               id, type_name<Component>());
      // Return a reference to a static dummy entity to avoid crash
      // This should never happen in proper usage, but prevents segfault
      static Entity dummy_entity;
      return dummy_entity;
    }
    auto *entity_ptr = singleton_map.at(id);
    if (!entity_ptr) {
      log_error("Singleton map contains null pointer for component {} ({})",
                id, type_name<Component>());
      static Entity dummy_entity;
      return dummy_entity;
    }
    return *entity_ptr;
  }

  template <typename Component> static Component *get_singleton_cmp() {
    Entity &ent = get_singleton<Component>();
    return &(ent.get<Component>());
  }

  static void markIDForCleanup(const int e_id) {
    const auto &entities = get_entities();
    auto it = entities.begin();
    while (it != get_entities().end()) {
      if ((*it)->id == e_id) {
        (*it)->cleanup = true;
        break;
      }
      it++;
    }
  }

  // Remove "pooled" components for an entity (components stored in a per-type
  // dense pool, rather than heap-allocated per-entity pointers).
  // This centralizes the "walk bitset, remove from ComponentStore, clear bit"
  // pattern used by cleanup/delete operations.
  static void remove_pooled_components_for(Entity &e) {
    for (ComponentID cid = 0; cid < max_num_components; ++cid) {
      if (!e.componentSet[cid])
        continue;
      e.componentSet[cid] = false;
      ComponentStore::get().remove_by_component_id(cid, e.id);
    }
  }

  static void cleanup() {
    EntityHelper::merge_entity_arrays();
    Entities &entities = get_entities_for_mod();

    std::size_t i = 0;
    while (i < entities.size()) {
      // `sp` is the current entity shared_ptr at index `i`.
      // (Entities are stored as `std::shared_ptr<Entity>` in `Entities`.)
      const auto &sp = entities[i];
      if (sp && !sp->cleanup) {
        ++i;
        continue;
      }
      if (sp) {
        remove_pooled_components_for(*sp);
      }
      // invalidate removed entity slot/id mapping
      invalidate_entity_slot_if_any(entities[i]);
      if (i != entities.size() - 1) {
        std::swap(entities[i], entities.back());
      }
      entities.pop_back();
    }

    // Treat cleanup as an end-of-frame boundary for component storage.
    ComponentStore::get().flush_end_of_frame();
  }

  static void delete_all_entities_NO_REALLY_I_MEAN_ALL() {
    auto &self = EntityHelper::get();
    Entities &entities = get_entities_for_mod();

    // Invalidate slots for all entities we currently know about.
    for (auto &sp : entities) {
      if (!sp)
        continue;
      remove_pooled_components_for(*sp);
      invalidate_entity_slot_if_any(sp);
    }
    for (auto &sp : self.temp_entities) {
      if (!sp)
        continue;
      remove_pooled_components_for(*sp);
      invalidate_entity_slot_if_any(sp);
    }

    entities.clear();
    self.temp_entities.clear();
    self.permanant_ids.clear();
    self.singletonMap.clear();

    ComponentStore::get().clear_all();
  }

  static void delete_all_entities(const bool include_permanent) {
    EntityHelper::merge_entity_arrays();

    if (include_permanent) {
      delete_all_entities_NO_REALLY_I_MEAN_ALL();
      return;
    }

    Entities &entities = get_entities_for_mod();

    const auto newend = std::remove_if(entities.begin(), entities.end(),
                                       [](const auto &entity) {
                                         return !EntityHelper::get()
                                                     .permanant_ids.contains(
                                                         entity->id);
                                       });

    // Anything being erased must also be removed from handle/component stores.
    for (auto it = newend; it != entities.end(); ++it) {
      const auto &sp = *it;
      if (!sp)
        continue;
      remove_pooled_components_for(*sp);
      invalidate_entity_slot_if_any(sp);
    }

    entities.erase(newend, entities.end());
    ComponentStore::get().flush_end_of_frame();
  }

  static void forEachEntity(const std::function<ForEachFlow(Entity &)> &cb) {
    for (const auto &e : get_entities()) {
      if (!e)
        continue;
      const auto fef = cb(*e);
      if (fef == 1)
        continue;
      if (fef == 2)
        break;
    }
  }

  static std::shared_ptr<Entity> getEntityAsSharedPtr(const Entity &entity) {
    for (const std::shared_ptr<Entity> &current_entity : get_entities()) {
      if (entity.id == current_entity->id)
        return current_entity;
    }
    return {};
  }

  static std::shared_ptr<Entity> getEntityAsSharedPtr(const OptEntity entity) {
    if (!entity)
      return {};
    const Entity &e = entity.asE();
    return getEntityAsSharedPtr(e);
  }

  static OptEntity getEntityForID(const EntityID id) {
    if (id == -1)
      return {};

    auto &self = EntityHelper::get();
    if (id >= 0) {
      const std::size_t idx = static_cast<std::size_t>(id);
      if (idx < self.id_to_slot.size()) {
        const EntityHandle::Slot slot = self.id_to_slot[idx];
        if (slot != EntityHandle::INVALID_SLOT && slot < self.slots.size()) {
          if (const auto &sp = self.slots[slot].ent; sp && sp->id == id) {
            return *sp;
          }
        }
      }
    }

    // Fallback (should be rare): linear scan.
    log_warn(
        "getEntityForID fallback scan for id={} (id_to_slot.size={}, slots.size={})",
        id, self.id_to_slot.size(), self.slots.size());
    for (const auto &e : get_entities()) {
      if (!e)
        continue;
      if (e->id == id) {
        log_warn("getEntityForID fallback hit: id={} entity_type={}", id,
                 e->entity_type);
        return *e;
      }
    }
    return {};
  }

  static Entity &getEntityForIDEnforce(const EntityID id) {
    auto opt_ent = getEntityForID(id);
    return opt_ent.asE();
  }

private:
  ComponentStore *store_ = nullptr;
  std::atomic_int *entity_id_gen_ = nullptr;
};

} // namespace afterhours
