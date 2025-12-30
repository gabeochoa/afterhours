#pragma once

#include <algorithm>
#include <functional>
#include <memory>
#include <set>
#include <vector>

#include "../debug_allocator.h"
#include "../singleton.h"
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

SINGLETON_FWD(EntityHelper)
struct EntityHelper {
    SINGLETON(EntityHelper)

    Entities entities_DO_NOT_USE;
    Entities temp_entities;
    std::set<int> permanant_ids;
    std::map<ComponentID, Entity *> singletonMap;

    // Compatibility-first design:
    // - `entities_DO_NOT_USE` remains the canonical dense list of live entities
    //   for iteration and for EntityQuery snapshots.
    // - Slots provide O(1) handle resolution and safe invalidation via
    // generation.
    struct Slot {
        EntityType ent{};
        uint32_t gen = 1;
        uint32_t index_into_dense = 0;
    };

    std::vector<Slot> slots;
    std::vector<uint32_t> free_list;

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

    // Default behavior (compatibility-first):
    // - Temp entities (pre-merge) have invalid handles (until merged).
    // - Invalid/stale handles resolve to null / empty.
    static EntityHandle handle_for(const Entity &e) {
        const uint32_t slot = e.ah_slot_index;
        if (slot == EntityHandle::INVALID_SLOT) {
            return EntityHandle::invalid();
        }

        auto &self = EntityHelper::get();
        if (slot >= self.slots.size()) {
            log_error(
                "handle_for: entity id {} has out-of-range slot {} (slots size "
                "{})",
                e.id, slot, self.slots.size());
            return EntityHandle::invalid();
        }

        const Slot &s = self.slots[slot];
        if (!s.ent || s.ent.get() != &e) {
            log_error(
                "handle_for: slot {} does not match entity id {} (slot ent: "
                "{}, slot ent id: {})",
                slot, e.id, s.ent ? "non-null" : "null",
                s.ent ? s.ent->id : -1);
            return EntityHandle::invalid();
        }

        return {slot, s.gen};
    }

    static OptEntity resolve(const EntityHandle h) {
        if (h.is_invalid()) {
            return {};
        }

        auto &self = EntityHelper::get();
        if (h.slot >= self.slots.size()) {
            return {};
        }

        Slot &s = self.slots[h.slot];
        if (s.gen != h.gen) {
            return {};
        }
        if (!s.ent) {
            return {};
        }
        return *s.ent;
    }

    static RefEntity resolveEnforced(const EntityHandle h) {
        OptEntity opt = resolve(h);
        if (!opt) {
            log_error(
                "resolveEnforced: EntityHandle is invalid or stale (slot={}, "
                "gen={})",
                h.slot, h.gen);
        }
        return opt.asE();
    }

    // Allocate a slot index for a newly-merged entity.
    // NOTE: generation is NOT reset on reuse; it is only incremented on delete.
    static uint32_t alloc_slot_index() {
        auto &self = EntityHelper::get();
        if (!self.free_list.empty()) {
            const uint32_t slot = self.free_list.back();
            self.free_list.pop_back();
            return slot;
        }
        self.slots.push_back(Slot{});
        return static_cast<uint32_t>(self.slots.size() - 1);
    }

    static uint32_t bump_gen(uint32_t gen) {
        gen += 1;
        // If `gen` overflows (uint32 wraparound), it can become 0. Avoid 0 to
        // keep "0 == default/uninitialized" out of the valid space.
        if (gen == 0) {
            gen = 1;
        }
        return gen;
    }

    static void invalidate_slot_for_entity_if_any(const EntityType &sp,
                                                  EntityHelper &self) {
        if (!sp) {
            return;
        }
        const uint32_t slot = sp->ah_slot_index;
        sp->ah_slot_index = EntityHandle::INVALID_SLOT;

        if (slot == EntityHandle::INVALID_SLOT) {
            return;
        }
        if (slot >= self.slots.size()) {
            log_error(
                "invalidate_slot_for_entity_if_any: entity id {} has "
                "out-of-range slot {} (slots size {})",
                sp->id, slot, self.slots.size());
            return;
        }

        Slot &s = self.slots[slot];
        if (!s.ent) {
            return;
        }
        s.ent.reset();
        s.gen = bump_gen(s.gen);
        self.free_list.push_back(slot);
    }

    // Dense removal that preserves handle bookkeeping:
    // - invalidates the removed entity's slot (bump generation + free slot)
    // - swap-removes from the dense `entities_DO_NOT_USE` vector
    // - updates the moved entity's slot back-pointer to its new dense index
    static void swap_remove_dense_index(Entities &entities, size_t i,
                                        EntityHelper &self) {
        invalidate_slot_for_entity_if_any(entities[i], self);

        if (i != entities.size() - 1) {
            std::swap(entities[i], entities.back());
            if (entities[i]) {
                const uint32_t moved_slot = entities[i]->ah_slot_index;
                if (moved_slot != EntityHandle::INVALID_SLOT &&
                    moved_slot < self.slots.size()) {
                    self.slots[moved_slot].index_into_dense =
                        static_cast<uint32_t>(i);
                }
            }
        }
        entities.pop_back();
    }

    static RefEntities get_ref_entities() {
        RefEntities matching;
        for (const auto &e : EntityHelper::get_entities()) {
            if (!e) continue;
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

        std::shared_ptr<Entity> e(new Entity());

#if defined(AFTER_HOURS_ASSIGN_HANDLES_ON_CREATE)
        // Opt-in behavior: assign a stable handle immediately, even while the
        // entity lives in `temp_entities`. Queries still won't see temp
        // entities unless force-merged, but `resolve(handle)` will work.
        {
            auto &self = EntityHelper::get();
            const uint32_t slot = alloc_slot_index();
            Slot &s = self.slots[slot];
            s.ent = e;
            s.index_into_dense = 0xFFFFFFFFu;  // not in dense list yet
            e->ah_slot_index = slot;
        }
#endif

        get_temp().push_back(e);

        if (options.is_permanent) {
            EntityHelper::get().permanant_ids.insert(e->id);
        }

        return *e;
    }

    static void merge_entity_arrays() {
        if (get_temp().empty()) return;

        auto &self = EntityHelper::get();
        for (const auto &entity : get_temp()) {
            if (!entity) continue;
            if (entity->cleanup) {
#if defined(AFTER_HOURS_ASSIGN_HANDLES_ON_CREATE)
                // If we assigned a handle on create, make sure to invalidate it
                // for temp entities that are never merged.
                invalidate_slot_for_entity_if_any(entity, self);
#endif
                continue;
            }

            get_entities_for_mod().push_back(entity);
            const uint32_t dense_index =
                static_cast<uint32_t>(get_entities_for_mod().size() - 1);

            uint32_t slot = entity->ah_slot_index;
            if (slot == EntityHandle::INVALID_SLOT) {
                slot = alloc_slot_index();
                entity->ah_slot_index = slot;
            }
            Slot &s = self.slots[slot];
            s.ent = entity;
            s.index_into_dense = dense_index;
        }
        get_temp().clear();
    }

    template<typename Component>
    static void registerSingleton(Entity &ent) {
        const ComponentID id = components::get_type_id<Component>();

        if (EntityHelper::get().singletonMap.contains(id)) {
            log_error("Already had registered singleton {}",
                      type_name<Component>());
        }

        EntityHelper::get().singletonMap.emplace(id, &ent);
        log_info("Registered singleton {} for {} ({})", ent.id,
                 type_name<Component>(), id);
    }

    template<typename Component>
    static RefEntity get_singleton() {
        const ComponentID id = components::get_type_id<Component>();
        auto &singleton_map = EntityHelper::get().singletonMap;
        if (!singleton_map.contains(id)) {
            log_warn(
                "Singleton map is missing value for component {} ({}). Did you "
                "register this component previously?",
                id, type_name<Component>());
            // Return a reference to a static dummy entity to avoid crash
            // This should never happen in proper usage, but prevents segfault
            static Entity dummy_entity;
            return dummy_entity;
        }
        auto *entity_ptr = singleton_map.at(id);
        if (!entity_ptr) {
            log_error(
                "Singleton map contains null pointer for component {} ({})", id,
                type_name<Component>());
            static Entity dummy_entity;
            return dummy_entity;
        }
        return *entity_ptr;
    }

    template<typename Component>
    static Component *get_singleton_cmp() {
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

    static void cleanup() {
        EntityHelper::merge_entity_arrays();
        Entities &entities = get_entities_for_mod();
        auto &self = EntityHelper::get();

        size_t i = 0;
        while (i < entities.size()) {
            const auto &sp = entities[i];
            if (sp && !sp->cleanup) {
                ++i;
                continue;
            }
            swap_remove_dense_index(entities, i, self);
        }
    }

    static void delete_all_entities_NO_REALLY_I_MEAN_ALL() {
        Entities &entities = get_entities_for_mod();
        auto &self = EntityHelper::get();

        for (auto &sp : entities) {
            if (sp) {
                sp->ah_slot_index = EntityHandle::INVALID_SLOT;
            }
        }
        for (auto &sp : self.temp_entities) {
            if (sp) {
                sp->ah_slot_index = EntityHandle::INVALID_SLOT;
            }
        }

        // Invalidate all slots that currently own an entity, and rebuild the
        // free list so future allocations can reuse slots safely (with bumped
        // gens).
        self.free_list.clear();
        self.free_list.reserve(self.slots.size());
        for (uint32_t slot = 0; slot < self.slots.size(); ++slot) {
            Slot &s = self.slots[slot];
            if (s.ent) {
                s.ent.reset();
                s.gen = bump_gen(s.gen);
            }
            self.free_list.push_back(slot);
        }

        entities.clear();
        self.temp_entities.clear();

        // Also reset auxiliary bookkeeping. This method is explicitly a "hard
        // reset", so it's appropriate to clear these too.
        self.permanant_ids.clear();
        self.singletonMap.clear();
    }

    static void delete_all_entities(const bool include_permanent) {
        EntityHelper::merge_entity_arrays();

        if (include_permanent) {
            delete_all_entities_NO_REALLY_I_MEAN_ALL();
            return;
        }

        Entities &entities = get_entities_for_mod();
        auto &self = EntityHelper::get();

        size_t i = 0;
        while (i < entities.size()) {
            const auto &sp = entities[i];
            if (!sp) {
                // Treat nulls as deletable.
            } else if (self.permanant_ids.contains(sp->id)) {
                ++i;
                continue;
            }
            swap_remove_dense_index(entities, i, self);
        }
    }

    static void forEachEntity(const std::function<ForEachFlow(Entity &)> &cb) {
        for (const auto &e : get_entities()) {
            if (!e) continue;
            const auto fef = cb(*e);
            if (fef == 1) continue;
            if (fef == 2) break;
        }
    }

    static std::shared_ptr<Entity> getEntityAsSharedPtr(const Entity &entity) {
        for (const std::shared_ptr<Entity> &current_entity : get_entities()) {
            if (entity.id == current_entity->id) return current_entity;
        }
        return {};
    }

    static std::shared_ptr<Entity> getEntityAsSharedPtr(
        const OptEntity entity) {
        if (!entity) return {};
        const Entity &e = entity.asE();
        return getEntityAsSharedPtr(e);
    }

    static OptEntity getEntityForID(const EntityID id) {
        if (id == -1) return {};

        for (const auto &e : get_entities()) {
            if (!e) continue;
            if (e->id == id) return *e;
        }
        return {};
    }

    static Entity &getEntityForIDEnforce(const EntityID id) {
        auto opt_ent = getEntityForID(id);
        return opt_ent.asE();
    }
};

}  // namespace afterhours
