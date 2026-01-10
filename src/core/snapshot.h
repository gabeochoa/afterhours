#pragma once

#include <optional>
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

inline std::vector<EntityRecord> take_entities(const Options &opt = {}) {
  if (opt.force_merge) {
    EntityHelper::merge_entity_arrays();
  }
  std::vector<EntityRecord> out;
  const Entities &ents = EntityHelper::get_entities();
  out.reserve(ents.size());
  for (const auto &sp : ents) {
    if (!sp)
      continue;
    const Entity &e = *sp;
    const EntityHandle h = EntityHelper::handle_for(e);
    if (!h.valid())
      continue;
    out.push_back(EntityRecord{
        .handle = h,
        .entity_type = e.entity_type,
        .tags = e.tags,
        .cleanup = e.cleanup,
    });
  }
  return out;
}

// Snapshot one component type into a pointer-free DTO type.
//
// This avoids copying components (which may be intentionally non-copyable).
// Instead, you provide a converter from `const Component&` -> `DTO`.
template <typename Component, typename DTO, typename Converter>
inline std::vector<ComponentRecord<DTO>>
take_components(Converter &&convert, const Options &opt = {}) {
  static_assert_pointer_free_types<DTO>();
  static_assert(std::is_copy_constructible_v<DTO>,
                "snapshot DTOs must be CopyConstructible");
  static_assert(std::is_base_of_v<BaseComponent, Component>,
                "Component must inherit from BaseComponent");

  if (opt.force_merge) {
    EntityHelper::merge_entity_arrays();
  }

  std::vector<ComponentRecord<DTO>> out;
  const Entities &ents = EntityHelper::get_entities();
  for (const auto &sp : ents) {
    if (!sp)
      continue;
    const Entity &e = *sp;
    if (!e.has<Component>())
      continue;
    const EntityHandle h = EntityHelper::handle_for(e);
    if (!h.valid())
      continue;
    out.push_back(ComponentRecord<DTO>{.entity = h,
                                       .value = convert(e.get<Component>())});
  }
  return out;
}

} // namespace afterhours::snapshot
