#pragma once

#include <functional>
#include <cstddef>
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

struct ApplySnapshotOptions {
  enum class MissingEntityPolicy {
    // Skip entries whose handles can't be resolved.
    Skip,
    // Create a new entity for entries whose handles can't be resolved.
    Create,
    // Treat unresolved handles as an error (no spawn, no apply for that entry).
    Error,
  };

  // If true, merge temp entities before applying so handles resolve.
  // This matches the default snapshot_for<>() behavior (force_merge view).
  bool force_merge = true;

  // If true, silently skip invalid handles rather than treating them as missing.
  bool skip_invalid_handles = true;

  // Policy for entries whose handles can't be resolved (stale/missing).
  MissingEntityPolicy missing_entity_policy = MissingEntityPolicy::Skip;

  // Back-compat alias for MissingEntityPolicy::Create.
  // If true, overrides missing_entity_policy to Create.
  //
  // NOTE: the new entity will not (and cannot) retain the original handle.
  bool create_missing_entities = false;

  // If true, merge newly created entities so they receive handles/slots.
  // Only relevant when missing entities are created.
  bool merge_new_entities = true;
};

struct ApplySnapshotResult {
  std::size_t applied = 0;
  std::size_t skipped_invalid_handle = 0;
  std::size_t skipped_unresolved = 0;
  std::size_t spawned = 0;
  std::size_t errors = 0;

  // First handle that triggered an error (e.g. unresolved + Error policy).
  EntityHandle first_error = EntityHandle::invalid();
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

// Apply a snapshot back onto the world by resolving handles and invoking an
// applier callback.
//
// This is intentionally generic: the snapshot "value" is often a pointer-free
// DTO produced by snapshot_for<T>(projector), not the component type itself.
template <typename V, typename ApplyFn>
[[nodiscard]] ApplySnapshotResult
apply_snapshot(const std::vector<std::pair<EntityHandle, V>> &snap,
               ApplyFn &&apply, const ApplySnapshotOptions options = {}) {
  static_assert_pointer_free_type<V>();
  static_assert(
      std::is_invocable_v<ApplyFn, Entity &, const V &>,
      "apply_snapshot requires an applier callable with signature: "
      "void(Entity&, const V&)");

  if (options.force_merge) {
    EntityHelper::merge_entity_arrays();
  }

  ApplySnapshotResult res{};
  const auto missing_policy =
      options.create_missing_entities ? ApplySnapshotOptions::MissingEntityPolicy::Create
                                      : options.missing_entity_policy;

  for (const auto &[h, v] : snap) {
    if (h.is_invalid()) {
      if (options.skip_invalid_handles) {
        ++res.skipped_invalid_handle;
        continue;
      }
    }

    auto resolved = EntityHelper::resolve(h);
    if (resolved.valid()) {
      apply(resolved.asE(), v);
      ++res.applied;
      continue;
    }

    if (missing_policy == ApplySnapshotOptions::MissingEntityPolicy::Skip) {
      ++res.skipped_unresolved;
      continue;
    }
    if (missing_policy == ApplySnapshotOptions::MissingEntityPolicy::Error) {
      ++res.errors;
      if (res.first_error.is_invalid()) {
        res.first_error = h;
      }
      continue;
    }

    // Create
    {
      Entity &e = EntityHelper::createEntity();
      apply(e, v);
      ++res.spawned;
    }
  }

  if (res.spawned > 0 &&
      (missing_policy == ApplySnapshotOptions::MissingEntityPolicy::Create) &&
      options.merge_new_entities) {
    EntityHelper::merge_entity_arrays();
  }

  return res;
}

// Variant of apply_snapshot that lets the caller control how missing entities
// are created (e.g. permanent entities, tagging, pre-wiring components, etc.).
template <typename V, typename ApplyFn, typename CreateEntityFn>
[[nodiscard]] ApplySnapshotResult
apply_snapshot(const std::vector<std::pair<EntityHandle, V>> &snap,
               ApplyFn &&apply, CreateEntityFn &&create_entity,
               const ApplySnapshotOptions options = {}) {
  static_assert_pointer_free_type<V>();
  static_assert(
      std::is_invocable_v<ApplyFn, Entity &, const V &>,
      "apply_snapshot requires an applier callable with signature: "
      "void(Entity&, const V&)");
  static_assert(
      std::is_invocable_r_v<Entity &, CreateEntityFn>,
      "apply_snapshot(snap, apply, create_entity) requires create_entity() -> "
      "Entity&");

  if (options.force_merge) {
    EntityHelper::merge_entity_arrays();
  }

  ApplySnapshotResult res{};
  const auto missing_policy =
      options.create_missing_entities ? ApplySnapshotOptions::MissingEntityPolicy::Create
                                      : options.missing_entity_policy;

  for (const auto &[h, v] : snap) {
    if (h.is_invalid()) {
      if (options.skip_invalid_handles) {
        ++res.skipped_invalid_handle;
        continue;
      }
    }

    auto resolved = EntityHelper::resolve(h);
    if (resolved.valid()) {
      apply(resolved.asE(), v);
      ++res.applied;
      continue;
    }

    if (missing_policy == ApplySnapshotOptions::MissingEntityPolicy::Skip) {
      ++res.skipped_unresolved;
      continue;
    }
    if (missing_policy == ApplySnapshotOptions::MissingEntityPolicy::Error) {
      ++res.errors;
      if (res.first_error.is_invalid()) {
        res.first_error = h;
      }
      continue;
    }

    // Create
    {
      Entity &e = create_entity();
      apply(e, v);
      ++res.spawned;
    }
  }

  if (res.spawned > 0 &&
      (missing_policy == ApplySnapshotOptions::MissingEntityPolicy::Create) &&
      options.merge_new_entities) {
    EntityHelper::merge_entity_arrays();
  }

  return res;
}

// Convenience: apply a "direct component snapshot" produced by snapshot_for<T>()
// (i.e. vector<pair<EntityHandle, Component>>).
template <typename T>
[[nodiscard]] ApplySnapshotResult
apply_snapshot_for(const Snapshot<T> &snap,
                   const ApplySnapshotOptions options = {})
  requires(std::is_copy_constructible_v<SnapshotComponent<T>>) {
  using C = SnapshotComponent<T>;
  return apply_snapshot<C>(
      snap,
      [](Entity &e, const C &value) {
        if (e.has<C>()) {
          if constexpr (std::is_copy_assignable_v<C>) {
            e.get<C>() = value;
          } else {
            e.removeComponent<C>();
            e.addComponent<C>(value);
          }
        } else {
          e.addComponent<C>(value);
        }
      },
      options);
}

// "World snapshot" convenience wrapper for direct component snapshots.
// This keeps the underlying persistence shape per-component, while making it
// easy to grab/apply multiple component snapshots at once.
template <typename... Ts>
[[nodiscard]] auto snapshot_world(const SnapshotOptions options = {}) {
  return std::tuple{snapshot_for<Ts>(options)...};
}

struct ApplyWorldResult {
  std::size_t applied = 0;
  std::size_t skipped_invalid_handle = 0;
  std::size_t skipped_unresolved = 0;
  std::size_t spawned = 0;
  std::size_t errors = 0;
};

inline ApplyWorldResult &operator+=(ApplyWorldResult &a,
                                   const ApplySnapshotResult &b) {
  a.applied += b.applied;
  a.skipped_invalid_handle += b.skipped_invalid_handle;
  a.skipped_unresolved += b.skipped_unresolved;
  a.spawned += b.spawned;
  a.errors += b.errors;
  return a;
}

template <typename... Ts>
[[nodiscard]] ApplyWorldResult
apply_world_for(const std::tuple<Snapshot<Ts>...> &world,
                const ApplySnapshotOptions options = {}) {
  ApplyWorldResult out{};
  std::apply(
      [&](const auto &...snaps) {
        ((out += apply_snapshot_for<Ts>(snaps, options)), ...);
      },
      world);
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

