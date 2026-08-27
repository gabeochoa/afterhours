#pragma once

#include <algorithm>
#include <functional>
#include <memory>
#include <set>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
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
  std::unordered_map<ComponentID, Entity *> singletonMap;

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

  // Entity pool: pre-allocated entities for reuse, avoiding heap churn.
  Entities entity_pool_;
  size_t max_pool_size_ = 0;
  std::vector<EntityID> free_ids_;

  struct CreationOptions {
    bool is_permanent;
  };

  // Bumped whenever an entity enters or leaves the slot table. An index built
  // at an older version is out of date.
  std::size_t version = 0;

  // Counters for the scaling test. An increment next to work that already
  // costs a virtual call or a std::function is not worth hiding behind a
  // define, and a claim about complexity nobody can count is just a claim.
  struct Stats {
    std::size_t index_considers = 0; // entities examined during a rebuild
    std::size_t index_rebuilds = 0;  // passes over the entity vector
    std::size_t bucket_resolves = 0; // handles turned back into entities
  };
  static Stats &stats() {
    static Stats s;
    return s;
  }

  // ---- Secondary indexes -------------------------------------------------
  // Rebuilt from the entity vector, never maintained incrementally. There is
  // no write interception on a component field (get<C>() hands out a mutable
  // reference), so an index kept up to date by hooks would go stale silently
  // the first time anyone assigned to the key. See invalidate_indexes().
  //
  // Buckets hold EntityHandle, not shared_ptr. A shared_ptr bucket would keep
  // a deleted entity alive with every component still attached, so a stale
  // bucket would hand back something indistinguishable from a live entity. A
  // handle fails the generation check and resolves to nothing instead.
  struct IndexBase {
    virtual ~IndexBase() = default;
    virtual void clear() = 0;
    virtual void consider(const Entity &e, EntityHandle h) = 0;
  };

  template <typename C, typename Key> struct Index : IndexBase {
    std::function<Key(const C &)> key_of;
    std::unordered_map<Key, std::vector<EntityHandle>> buckets;

    void clear() override { buckets.clear(); }
    void consider(const Entity &e, EntityHandle h) override {
      ++stats().index_considers;
      if (!e.has<C>())
        return;
      buckets[key_of(e.get<C>())].push_back(h);
    }
  };

  std::unordered_map<ComponentID, std::unique_ptr<IndexBase>> indexes;
  std::size_t indexed_version = static_cast<std::size_t>(-1);

  // Register once at startup. The key is whatever key_fn returns.
  template <typename C, typename KeyFn> void add_index(KeyFn key_fn) {
    using Key = std::decay_t<std::invoke_result_t<KeyFn, const C &>>;
    auto idx = std::make_unique<Index<C, Key>>();
    idx->key_of = std::move(key_fn);
    indexes[components::get_type_id<C>()] = std::move(idx);
    invalidate_indexes();
  }

  // Version gating sees entities appear and disappear. It cannot see a write
  // to an indexed field, because nothing in the ECS can. Call this after
  // assigning one.
  void invalidate_indexes() {
    indexed_version = static_cast<std::size_t>(-1);
  }

  bool indexes_are_fresh() const {
    return indexes.empty() || indexed_version == version;
  }

  // One pass over the entities feeds every registered index. Doing it per
  // index is what the hand-rolled versions downstream ended up paying.
  void ensure_indexes_fresh() {
    if (indexes_are_fresh())
      return;
    for (auto &[_, idx] : indexes)
      idx->clear();
    ++stats().index_rebuilds;
    for (const auto &sp : entities_DO_NOT_USE) {
      if (!sp || sp->cleanup)
        continue;
      const EntityHandle h = handle_for(*sp);
      if (h.is_invalid())
        continue;
      for (auto &[_, idx] : indexes)
        idx->consider(*sp, h);
    }
    indexed_version = version;
  }

  template <typename C, typename Key>
  const std::vector<EntityHandle> &indexed(const Key &key) {
    static const std::vector<EntityHandle> none;
    ensure_indexes_fresh();

    const auto it = indexes.find(components::get_type_id<C>());
    if (it == indexes.end()) {
      log_error("indexed<{}>: no index registered, call add_index first",
                type_name<C>());
      return none;
    }
    // dynamic_cast rather than static_cast: asking with a key type the index
    // was not registered with is a silent wrong answer otherwise.
    auto *idx = dynamic_cast<Index<C, Key> *>(it->second.get());
    if (!idx) {
      log_error("indexed<{}>: key type does not match the registered index",
                type_name<C>());
      return none;
    }
    const auto found = idx->buckets.find(key);
    return found == idx->buckets.end() ? none : found->second;
  }

  // Bump a slot generation counter so old handles become stale.
  // Returns a non-zero generation (wraparound skips 0).
  static EntityHandle::Slot bump_gen(EntityHandle::Slot gen) {
    // Unsigned wraparound is well-defined; if we wrapped to 0, bump to 1.
    const EntityHandle::Slot next = gen + 1;
    return next + static_cast<EntityHandle::Slot>(next == 0);
  }

  void reserve_temp_space() { temp_entities.reserve(100); }

  void reserve_entities(size_t count) {
    max_pool_size_ = std::max(max_pool_size_, count * 2);
    entity_pool_.reserve(count);
    for (size_t i = 0; i < count; ++i) {
      entity_pool_.push_back(std::make_shared<Entity>(EntityID{-1}));
    }
  }

  size_t pool_size() const { return entity_pool_.size(); }

  EntityID alloc_entity_id() {
    if (!free_ids_.empty()) {
      EntityID id = free_ids_.back();
      free_ids_.pop_back();
      return id;
    }
    return ENTITY_ID_GEN++;
  }

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
    // An entity entered the slot table, so any index built before now is short
    // one row. Bumping here rather than at each caller means a path added later
    // cannot forget to.
    ++version;

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
    // An entity left, so an index built before now holds a row that resolves
    // to nothing. The same reason as assign_slot_to_entity above.
    ++version;
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

    EntityType e;
    if (!entity_pool_.empty()) {
      e = std::move(entity_pool_.back());
      entity_pool_.pop_back();
      e->recycle(alloc_entity_id());
    } else {
      e = std::make_shared<Entity>(alloc_entity_id());
    }
    temp_entities.push_back(e);
    // Identity does not wait for the merge: handle_for works immediately, so a
    // caller building an entity and its children needs no mid-frame merge.
    assign_slot_to_entity(e);

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
      if (entity->cleanup) {
        // Dropped here rather than reaching cleanup()'s sweep, so this is the
        // only place its slot can be released.
        invalidate_entity_slot_if_any(entity);
        continue;
      }
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
      // Warn once per component id, not every frame — a missing singleton is
      // queried per-frame and would otherwise flood the log.
      static std::unordered_set<ComponentID> warned;
      if (warned.insert(id).second) {
        log_warn("Singleton map is missing value for component {} ({}). Did "
                 "you register this component previously?",
                 id, type_name<Component>());
      }
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

  // Null when there is no such singleton. It used to reach that null by taking
  // the address of a downcast of a null component on the dummy entity, which
  // is UB that clang and g++ happened to turn into the right answer. A build
  // with UBSan on, which is what zig c++ gives you in Debug, trapped on the
  // first singleton queried before registration and killed the process.
  // Callers that require the singleton want get_singleton_cmp_enforce, which
  // logs and aborts.
  template <typename Component> Component *get_singleton_cmp() const {
    Entity &ent = get_singleton<Component>();
    if (!ent.has<Component>())
      return nullptr;
    return &(ent.get<Component>());
  }

  template <typename Component>
  const Component *get_singleton_cmp_const() const {
    Entity &ent = get_singleton<Component>();
    if (!ent.has<Component>())
      return nullptr;
    return &(ent.get<Component>());
  }

  template <typename Component> Component &get_singleton_cmp_enforce() const {
    const ComponentID id = components::get_type_id<Component>();
    if (!singletonMap.contains(id)) {
      log_error("get_singleton_cmp_enforce: Missing required singleton {} ({})",
                id, type_name<Component>());
      std::abort();
    }
    Entity &ent = *singletonMap.at(id);
    return ent.get<Component>();
  }

  template <typename Component> bool has_singleton() const {
    const ComponentID id = components::get_type_id<Component>();
    return singletonMap.contains(id);
  }

  void markIDForCleanup(const int e_id) {
    if (e_id < 0)
      return;
    // Fast path: use the id_to_slot mapping for O(1) lookup.
    const auto idx = static_cast<std::size_t>(e_id);
    if (idx < id_to_slot.size()) {
      const auto slot = id_to_slot[idx];
      if (slot != EntityHandle::INVALID_SLOT && slot < slots.size()) {
        if (auto &sp = slots[slot].ent; sp && sp->id == e_id) {
          sp->cleanup = true;
          return;
        }
      }
    }
    // Fallback: linear scan (handles edge cases where id_to_slot is stale).
    for (const auto &sp : get_entities()) {
      if (sp && sp->id == e_id) {
        sp->cleanup = true;
        return;
      }
    }
  }

  void cleanup() {
    merge_entity_arrays();
    Entities &entities = get_entities_for_mod();

    // Build a set of entity pointers that are registered as singletons.
    // This lets us skip the singletonMap scan for the vast majority of
    // entities that are not singletons.
    std::unordered_set<Entity *> singleton_entities;
    singleton_entities.reserve(singletonMap.size());
    for (const auto &[_, ptr] : singletonMap) {
      singleton_entities.insert(ptr);
    }

    // Stable compaction: survivors keep their relative order. Read cursor
    // scans, write cursor packs, and the tail is dropped once at the end.
    std::size_t write = 0;
    for (std::size_t read = 0; read < entities.size(); ++read) {
      const auto &sp = entities[read];
      if (sp && !sp->cleanup) {
        if (write != read)
          entities[write] = std::move(entities[read]);
        ++write;
        continue;
      }
      // Remove any singleton registrations pointing at this entity,
      // but only if this entity is actually a singleton.
      if (sp && singleton_entities.count(sp.get())) {
        Entity *removed_ptr = sp.get();
        for (auto it = singletonMap.begin(); it != singletonMap.end();) {
          if (it->second == removed_ptr) {
            it = singletonMap.erase(it);
          } else {
            ++it;
          }
        }
        singleton_entities.erase(removed_ptr);
      }
      // invalidate removed entity slot/id mapping
      invalidate_entity_slot_if_any(entities[read]);

      EntityType removed = std::move(entities[read]);
      if (removed) {
        EntityID old_id = removed->id;
        if (old_id >= 0) {
          free_ids_.push_back(old_id);
        }
        if (entity_pool_.size() < max_pool_size_) {
          entity_pool_.push_back(std::move(removed));
        }
      }
    }
    entities.resize(write);
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

    // Build singleton membership set for fast skip.
    std::unordered_set<Entity *> singleton_entities;
    singleton_entities.reserve(singletonMap.size());
    for (const auto &[_, ptr] : singletonMap) {
      singleton_entities.insert(ptr);
    }

    std::size_t i = 0;
    while (i < entities.size()) {
      const auto &sp = entities[i];
      const bool keep = sp && permanant_ids.contains(sp->id);
      if (keep) {
        ++i;
        continue;
      }

      // Remove singleton registrations only if this entity is a singleton.
      if (sp && singleton_entities.count(sp.get())) {
        Entity *removed = sp.get();
        for (auto it = singletonMap.begin(); it != singletonMap.end();) {
          if (it->second == removed) {
            it = singletonMap.erase(it);
          } else {
            ++it;
          }
        }
        singleton_entities.erase(removed);
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
    // Every handle recorded anywhere is about to mean something else, and
    // clearing to an empty entity list would otherwise bump nothing.
    ++version;
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

  // Fast path is the O(1) id_to_slot lookup; the scan is a fallback for
  // entities whose slot mapping is stale or absent (e.g. not yet merged).
  std::shared_ptr<Entity> getEntityAsSharedPtr(const Entity &entity) const {
    const EntityID id = entity.id;
    if (id >= 0) {
      const std::size_t idx = static_cast<std::size_t>(id);
      if (idx < id_to_slot.size()) {
        const EntityHandle::Slot slot = id_to_slot[idx];
        if (slot != EntityHandle::INVALID_SLOT && slot < slots.size()) {
          if (const auto &sp = slots[slot].ent; sp && sp->id == id)
            return sp;
        }
      }
    }
    for (const std::shared_ptr<Entity> &current_entity : get_entities()) {
      if (entity.id == current_entity->id)
        return current_entity;
    }
    return {};
  }
};

} // namespace afterhours
