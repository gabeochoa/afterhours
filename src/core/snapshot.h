#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
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

// Canonical (allocation/creation-order independent) snapshots: entities are
// keyed by a caller-supplied stable identity, never by runtime handle.
using Field = std::pair<std::string, std::string>;

struct CanonicalEntity {
  std::string identity;
  int entity_type = 0;
  TagBitset tags{};
  bool cleanup = false;
  std::vector<Field> fields; // sorted by (name, value) after canonicalize/add
};

struct CanonicalSnapshot {
  std::vector<CanonicalEntity> entities; // sorted by identity
  [[nodiscard]] std::string encode() const;
  [[nodiscard]] std::uint64_t hash() const; // FNV-1a 64 over encode()
};

struct SnapshotDifference {
  std::string identity;
  std::string field; // "<entity>" when present on one side only
  std::string left;
  std::string right;
  [[nodiscard]] std::string describe() const {
    return "entity '" + identity + "' field '" + field + "': '" + left +
           "' != '" + right + "'";
  }
};

namespace detail {
inline void append_token(std::string &out, const std::string &v) {
  out += std::to_string(v.size());
  out += ':';
  out += v;
}
inline std::uint64_t fnv1a(const std::string &s) {
  std::uint64_t h = 14695981039346656037ull;
  for (unsigned char c : s) { h ^= c; h *= 1099511628211ull; }
  return h;
}
inline void sort_fields(std::vector<Field> &f) {
  std::sort(f.begin(), f.end());
}
} // namespace detail

inline std::string CanonicalSnapshot::encode() const {
  std::string out = "ah-snapshot-v1;";
  for (const auto &e : entities) {
    detail::append_token(out, e.identity);
    detail::append_token(out, std::to_string(e.entity_type));
    detail::append_token(out, e.tags.to_string());
    detail::append_token(out, e.cleanup ? "1" : "0");
    out += std::to_string(e.fields.size());
    out += ';';
    for (const auto &f : e.fields) {
      detail::append_token(out, f.first);
      detail::append_token(out, f.second);
    }
  }
  return out;
}
inline std::uint64_t CanonicalSnapshot::hash() const {
  return detail::fnv1a(encode());
}

// IdentityFn: std::string(const EntityHandle &). Throws on empty/duplicate.
template <typename IdentityFn>
CanonicalSnapshot canonicalize(const std::vector<EntityRecord> &records,
                               IdentityFn &&identity_of) {
  CanonicalSnapshot out;
  out.entities.reserve(records.size());
  for (const auto &r : records) {
    std::string id = identity_of(r.handle);
    if (id.empty())
      throw std::invalid_argument("snapshot: empty stable identity");
    out.entities.push_back(CanonicalEntity{.identity = std::move(id),
                                           .entity_type = r.entity_type,
                                           .tags = r.tags,
                                           .cleanup = r.cleanup,
                                           .fields = {}});
  }
  std::sort(out.entities.begin(), out.entities.end(),
            [](const CanonicalEntity &a, const CanonicalEntity &b) {
              return a.identity < b.identity;
            });
  for (std::size_t i = 1; i < out.entities.size(); i++)
    if (out.entities[i - 1].identity == out.entities[i].identity)
      throw std::invalid_argument("snapshot: duplicate stable identity '" +
                                  out.entities[i].identity + "'");
  return out;
}

// FieldsFn: std::vector<Field>(const DTO &). Names are prefixed "<name>.".
template <typename DTO, typename IdentityFn, typename FieldsFn>
void add_components(CanonicalSnapshot &snap,
                    const std::vector<ComponentRecord<DTO>> &records,
                    IdentityFn &&identity_of, const std::string &name,
                    FieldsFn &&fields_of) {
  std::unordered_map<std::string, std::size_t> by_id;
  for (std::size_t i = 0; i < snap.entities.size(); i++)
    by_id[snap.entities[i].identity] = i;
  for (const auto &r : records) {
    const std::string id = identity_of(r.entity);
    auto it = by_id.find(id);
    if (it == by_id.end())
      throw std::invalid_argument("snapshot: component for unknown identity '" +
                                  id + "'");
    auto &fields = snap.entities[it->second].fields;
    for (auto &f : fields_of(r.value))
      fields.emplace_back(name + "." + f.first, std::move(f.second));
  }
  for (auto &e : snap.entities) detail::sort_fields(e.fields);
}

inline std::optional<SnapshotDifference>
first_difference(const CanonicalSnapshot &a, const CanonicalSnapshot &b) {
  std::size_t i = 0, j = 0;
  while (i < a.entities.size() || j < b.entities.size()) {
    if (j == b.entities.size() ||
        (i < a.entities.size() && a.entities[i].identity < b.entities[j].identity))
      return SnapshotDifference{a.entities[i].identity, "<entity>", "present", "missing"};
    if (i == a.entities.size() || b.entities[j].identity < a.entities[i].identity)
      return SnapshotDifference{b.entities[j].identity, "<entity>", "missing", "present"};
    const auto &x = a.entities[i];
    const auto &y = b.entities[j];
    auto diff = [&](const std::string &f, std::string l, std::string r) {
      return SnapshotDifference{x.identity, f, std::move(l), std::move(r)};
    };
    if (x.entity_type != y.entity_type)
      return diff("entity_type", std::to_string(x.entity_type), std::to_string(y.entity_type));
    if (x.tags != y.tags) return diff("tags", x.tags.to_string(), y.tags.to_string());
    if (x.cleanup != y.cleanup)
      return diff("cleanup", x.cleanup ? "1" : "0", y.cleanup ? "1" : "0");
    std::size_t p = 0, q = 0;
    while (p < x.fields.size() || q < y.fields.size()) {
      if (q == y.fields.size() || (p < x.fields.size() && x.fields[p] < y.fields[q])) {
        if (q < y.fields.size() && y.fields[q].first == x.fields[p].first)
          return diff(x.fields[p].first, x.fields[p].second, y.fields[q].second);
        return diff(x.fields[p].first, x.fields[p].second, "<missing>");
      }
      if (p == x.fields.size() || y.fields[q] < x.fields[p]) {
        if (p < x.fields.size() && x.fields[p].first == y.fields[q].first)
          return diff(y.fields[q].first, x.fields[p].second, y.fields[q].second);
        return diff(y.fields[q].first, "<missing>", y.fields[q].second);
      }
      if (x.fields[p].second != y.fields[q].second)
        return diff(x.fields[p].first, x.fields[p].second, y.fields[q].second);
      p++; q++;
    }
    i++; j++;
  }
  return std::nullopt;
}

} // namespace afterhours::snapshot
