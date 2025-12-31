#pragma once

#include <cstdint>
#include <limits>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

#include "base_component.h"
#include "entity_id.h"

namespace afterhours {

// Dense component pool with O(1) entity lookup.
//
// Default mode:
// - removal uses swap-remove (fast, but component references can be invalidated)
//
// EOF stability mode (`AFTER_HOURS_KEEP_REFERENCES_UNTIL_EOF`):
// - removal leaves tombstones and defers compaction until `flush_end_of_frame()`
// - references remain valid until the explicit flush boundary
//
template <typename T> class ComponentPool {
  static_assert(std::is_base_of_v<BaseComponent, T>,
                "ComponentPool<T> requires T to inherit BaseComponent");

public:
  using DenseIndex = std::uint32_t;
  static constexpr DenseIndex INVALID_INDEX =
      (std::numeric_limits<DenseIndex>::max)();

  // Used by Entity/Query fast paths to check component presence for an entity.
  [[nodiscard]] bool has(const EntityID id) const {
    if (id < 0)
      return false;
    const auto idx = static_cast<std::size_t>(id);
    return idx < entity_to_dense_.size() && entity_to_dense_[idx] != INVALID_INDEX;
  }

  // Used by RTTI/derived queries and internal callers that need nullable access.
  [[nodiscard]] T *try_get(const EntityID id) {
    if (!has(id))
      return nullptr;
    const DenseIndex di = entity_to_dense_[static_cast<std::size_t>(id)];
    return &dense_[static_cast<std::size_t>(di)];
  }

  // Const version of `try_get()` for read-only access paths.
  [[nodiscard]] const T *try_get(const EntityID id) const {
    if (!has(id))
      return nullptr;
    const DenseIndex di = entity_to_dense_[static_cast<std::size_t>(id)];
    return &dense_[static_cast<std::size_t>(di)];
  }

  // Used by `Entity::get<T>()` once presence has been validated.
  [[nodiscard]] T &get(const EntityID id) {
    // Caller is expected to have validated presence via Entity::has<T>().
    return *try_get(id);
  }

  // Const `get()` used by `Entity::get<T>() const`.
  [[nodiscard]] const T &get(const EntityID id) const { return *try_get(id); }

  // Used by `Entity::addComponent<T>()` to construct/attach a component in-place.
  template <typename... Args> T &emplace(const EntityID id, Args &&...args) {
    ensure_entity_mapping_size(id);

    const auto idx = static_cast<std::size_t>(id);
    if (entity_to_dense_[idx] != INVALID_INDEX) {
      // Already exists; behave like add-if-missing.
      return dense_[static_cast<std::size_t>(entity_to_dense_[idx])];
    }

    const DenseIndex dense_index = static_cast<DenseIndex>(dense_.size());
    dense_.emplace_back(std::forward<Args>(args)...);
    dense_to_entity_.push_back(id);
    entity_to_dense_[idx] = dense_index;
    return dense_.back();
  }

  // Used by `Entity::removeComponent<T>()` and entity cleanup/delete paths.
  void remove(const EntityID id) {
    if (!has(id))
      return;

    const auto id_idx = static_cast<std::size_t>(id);
    const DenseIndex di = entity_to_dense_[id_idx];
    entity_to_dense_[id_idx] = INVALID_INDEX;

#if defined(AFTER_HOURS_KEEP_REFERENCES_UNTIL_EOF)
    // Tombstone; defer compaction so references stay valid until flush.
    dense_to_entity_[static_cast<std::size_t>(di)] = INVALID_ENTITY_ID;
    pending_removals_.push_back(di);
#else
    // Swap-remove for density.
    const DenseIndex last = static_cast<DenseIndex>(dense_.size() - 1);
    if (di != last) {
      // Avoid requiring move-assignment on components (many components inherit
      // from BaseComponent, which intentionally doesn't provide assignment).
      T *dst = &dense_[static_cast<std::size_t>(di)];
      T *src = &dense_[static_cast<std::size_t>(last)];
      dst->~T();
      new (dst) T(std::move(*src));
      const EntityID moved_entity = dense_to_entity_[static_cast<std::size_t>(last)];
      dense_to_entity_[static_cast<std::size_t>(di)] = moved_entity;
      if (moved_entity >= 0) {
        entity_to_dense_[static_cast<std::size_t>(moved_entity)] = di;
      }
    }
    dense_.pop_back();
    dense_to_entity_.pop_back();
#endif
  }

  // Used by "reset world" operations to drop all components of this type.
  void clear() {
    dense_.clear();
    dense_to_entity_.clear();
    entity_to_dense_.clear();
#if defined(AFTER_HOURS_KEEP_REFERENCES_UNTIL_EOF)
    pending_removals_.clear();
#endif
  }

  // Used as an explicit flush boundary for EOF-stability mode compaction.
  void flush_end_of_frame() {
#if defined(AFTER_HOURS_KEEP_REFERENCES_UNTIL_EOF)
    if (pending_removals_.empty())
      return;

    std::vector<T> new_dense;
    std::vector<EntityID> new_dense_to_entity;
    new_dense.reserve(dense_.size() - pending_removals_.size());
    new_dense_to_entity.reserve(dense_.size() - pending_removals_.size());

    // Rebuild dense arrays (compaction). This invalidates references, which is
    // allowed at the explicit EOF flush boundary.
    for (std::size_t i = 0; i < dense_.size(); ++i) {
      const EntityID ent = dense_to_entity_[i];
      if (ent == INVALID_ENTITY_ID)
        continue;
      const DenseIndex new_index = static_cast<DenseIndex>(new_dense.size());
      new_dense.emplace_back(std::move(dense_[i]));
      new_dense_to_entity.push_back(ent);
      if (ent >= 0) {
        ensure_entity_mapping_size(ent);
        entity_to_dense_[static_cast<std::size_t>(ent)] = new_index;
      }
    }

    dense_.swap(new_dense);
    dense_to_entity_.swap(new_dense_to_entity);
    pending_removals_.clear();
#endif
  }

private:
  static constexpr EntityID INVALID_ENTITY_ID = -1;

  // Used to grow `entity_to_dense_` so `EntityID` can index it safely.
  void ensure_entity_mapping_size(const EntityID id) {
    if (id < 0)
      return;
    const auto need = static_cast<std::size_t>(id) + 1;
    if (entity_to_dense_.size() < need) {
      entity_to_dense_.resize(need, INVALID_INDEX);
    }
  }

  std::vector<T> dense_;
  std::vector<EntityID> dense_to_entity_;
  std::vector<DenseIndex> entity_to_dense_;

#if defined(AFTER_HOURS_KEEP_REFERENCES_UNTIL_EOF)
  std::vector<DenseIndex> pending_removals_;
#endif
};

} // namespace afterhours

