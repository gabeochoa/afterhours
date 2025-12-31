#pragma once

#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "entity.h"
#include "entity_helper.h"
#include "pointer_policy.h"

namespace afterhours::snapshot {

struct Options {
  // If true, ensure temp entities are merged before snapshotting.
  bool force_merge = true;
};

struct EntityRecord {
  EntityHandle handle{};
  int entity_type = 0;
  TagBitset tags{};
  bool cleanup = false;
};

template <typename T> struct ComponentRecord {
  EntityHandle entity{};
  T value{};
};

template <typename... Components> struct Snapshot {
  std::vector<EntityRecord> entities;
  std::tuple<std::vector<ComponentRecord<Components>>...> components;
};

namespace detail {
template <typename T>
inline void append_component_records(std::vector<ComponentRecord<T>> &out,
                                     const Entities &ents) {
  static_assert(std::is_copy_constructible_v<T>,
                "snapshot requires copyable component types (T must be "
                "CopyConstructible). If you need move-only components, "
                "snapshot them explicitly via a separate DTO.");

  for (const auto &sp : ents) {
    if (!sp)
      continue;
    const Entity &e = *sp;
    if (!e.has<T>())
      continue;
    const EntityHandle h = EntityHelper::handle_for(e);
    if (!h.valid())
      continue;
    out.push_back(ComponentRecord<T>{.entity = h, .value = e.get<T>()});
  }
}
} // namespace detail

// Take a pointer-free snapshot of the world for a chosen component set.
//
// This is intentionally explicit: the caller chooses which components are part
// of the snapshot surface. A compile-time policy rejects pointer-like types.
template <typename... Components>
inline Snapshot<Components...> take(const Options &opt = {}) {
  static_assert_pointer_free_types<Components...>();

  if (opt.force_merge) {
    EntityHelper::merge_entity_arrays();
  }

  Snapshot<Components...> s;

  const Entities &ents = EntityHelper::get_entities();
  s.entities.reserve(ents.size());

  for (const auto &sp : ents) {
    if (!sp)
      continue;
    const Entity &e = *sp;
    const EntityHandle h = EntityHelper::handle_for(e);
    if (!h.valid())
      continue;
    s.entities.push_back(EntityRecord{
        .handle = h, .entity_type = e.entity_type, .tags = e.tags, .cleanup = e.cleanup});
  }

  // Fill per-component arrays.
  (detail::append_component_records<Components>(
       std::get<std::vector<ComponentRecord<Components>>>(s.components), ents),
   ...);

  return s;
}

} // namespace afterhours::snapshot

