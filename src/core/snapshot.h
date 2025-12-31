#pragma once

#include <functional>
#include <type_traits>
#include <utility>
#include <vector>

#include "../logging.h"
#include "../type_name.h"
#include "entity_handle.h"
#include "entity_helper.h"
#include "entity_query.h"
#include "pointer_policy.h"

namespace afterhours {

// Snapshot API (pointer-free export)
// - **What**: materialize `(EntityHandle, value)` pairs for entities with a component.
// - **Why**: safe surface for save/load, replay, debug capture (no live pointers).
// - **How**: prefer `snapshot_for<Component>(projector)` where `projector` returns a
//   small, copyable, pointer-free DTO. Many ECS components are intentionally non-copyable.
//
// Pointer-free snapshot surface:
// - Materializes a stable list of (EntityHandle, component_value) pairs.
// - Intended for serialization/debug capture/replay without embedding pointers.
//
// NOTE: This only guards the top-level component type `T` against pointer-like
// types (raw/smart pointers, RefEntity, OptEntity, etc.). It does not recursively
// inspect member fields.

struct SnapshotOptions {
  // If true, drop entities that don't have a valid EntityHandle (e.g. temp
  // entities before merge). This is usually what you want for persisted data.
  bool skip_invalid_handles = true;
};

template <typename T>
using SnapshotComponent = std::remove_cvref_t<T>;

template <typename T>
using Snapshot = std::vector<std::pair<EntityHandle, SnapshotComponent<T>>>;

template <typename T>
[[nodiscard]] Snapshot<T> snapshot_for(const EntityQuery<> &query,
                                       const SnapshotOptions options = {})
  requires(std::is_copy_constructible_v<SnapshotComponent<T>>) {
  using C = SnapshotComponent<T>;
  static_assert_pointer_free_type<C>();

  Snapshot<T> out;

  // We can't precompute an exact size without a second pass (we filter by has<T>),
  // but reserving the upper bound avoids repeated reallocation.
  const auto ents = query.gen();
  out.reserve(ents.size());

  for (Entity &e : ents) {
    if (!e.has<C>())
      continue;

    const EntityHandle h = EntityHelper::handle_for(e);
    if (h.is_invalid()) {
      if (!options.skip_invalid_handles) {
        out.emplace_back(h, e.get<C>());
      } else {
        log_warn("snapshot_for<{}>: skipping entity id={} without valid handle "
                 "(did you forget to merge?)",
                 type_name<C>(), e.id);
      }
      continue;
    }

    out.emplace_back(h, e.get<C>());
  }

  return out;
}

// Snapshot a component type `T` by projecting it to a pointer-free, copyable
// value type.
//
// This is the recommended form for snapshotting ECS components since many
// components are intentionally non-copyable (e.g. via BaseComponent).
template <typename T, typename ProjectFn>
[[nodiscard]] auto snapshot_for(const EntityQuery<> &query, ProjectFn &&project,
                                const SnapshotOptions options = {}) {
  using C = SnapshotComponent<T>;
  using V = std::remove_cvref_t<std::invoke_result_t<ProjectFn, const C &>>;
  static_assert_pointer_free_type<V>();
  static_assert(std::is_copy_constructible_v<V>,
                "snapshot_for<T>(projector) requires the projected value type to "
                "be copy-constructible");

  std::vector<std::pair<EntityHandle, V>> out;

  const auto ents = query.gen();
  out.reserve(ents.size());

  for (Entity &e : ents) {
    if (!e.has<C>())
      continue;

    const EntityHandle h = EntityHelper::handle_for(e);
    if (h.is_invalid()) {
      if (!options.skip_invalid_handles) {
        out.emplace_back(h, project(e.get<C>()));
      } else {
        log_warn("snapshot_for<{}>: skipping entity id={} without valid handle "
                 "(did you forget to merge?)",
                 type_name<C>(), e.id);
      }
      continue;
    }

    out.emplace_back(h, project(e.get<C>()));
  }

  return out;
}

template <typename T>
[[nodiscard]] Snapshot<T> snapshot_for(const SnapshotOptions options = {}) {
  // Default to a force-merged view so handles exist and temp entities are included.
  EntityQuery<> q({.force_merge = true, .ignore_temp_warning = true});
  return snapshot_for<T>(q, options);
}

template <typename T, typename ProjectFn>
[[nodiscard]] auto snapshot_for(ProjectFn &&project,
                                const SnapshotOptions options = {}) {
  EntityQuery<> q({.force_merge = true, .ignore_temp_warning = true});
  return snapshot_for<T>(q, std::forward<ProjectFn>(project), options);
}

} // namespace afterhours

