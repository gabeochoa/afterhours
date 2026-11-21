#pragma once

// One attachment relation instead of per-pair held-item/furniture/rope
// bookkeeping: a child names its single parent, an optional socket, and a
// local pose; its world pose is composed from the parent's on demand.
// System-free: games call the helpers where they already update transforms.

#include <cmath>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "core/base_component.h"
#include "core/entity.h"
#include "core/entity_collection.h"

namespace afterhours {

struct Pose2D {
  float x = 0.f, y = 0.f, angle_deg = 0.f;
};

// An entity's own (unattached) pose; a parent's pose feeds composition.
struct HasPose2D : BaseComponent {
  Pose2D pose;
  HasPose2D() = default;
  explicit HasPose2D(Pose2D p) : pose(p) {}
  HasPose2D(float x, float y, float a = 0.f) : pose{x, y, a} {}
};

struct AttachedTo : BaseComponent {
  EntityID parent = -1;
  std::string socket; // opaque name, e.g. "hand_r"; empty = parent origin
  Pose2D local;       // offset in parent space
  [[nodiscard]] bool attached() const { return parent >= 0; }
};

namespace attachment {

using Filter = std::function<bool(const Entity &child, const Entity &parent)>;

inline Pose2D compose(const Pose2D &parent, const Pose2D &local) {
  const float r = parent.angle_deg * 3.14159265358979323846f / 180.f;
  const float c = std::cos(r), s = std::sin(r);
  return {parent.x + local.x * c - local.y * s,
          parent.y + local.x * s + local.y * c, parent.angle_deg + local.angle_deg};
}

inline Entity *find(EntityCollection &ec, EntityID id) {
  for (auto &sp : ec.get_entities_for_mod())
    if (sp && sp->id == id) return sp.get();
  return nullptr;
}
inline const Entity *find(const EntityCollection &ec, EntityID id) {
  for (auto &sp : ec.get_entities())
    if (sp && sp->id == id) return sp.get();
  return nullptr;
}

// Attaching a child that is already attached moves it (single-parent
// invariant). Returns false, leaving any old attachment intact, on
// self-attach, a would-be cycle, or filter rejection.
inline bool attach(Entity &child, Entity &parent, Pose2D local = {},
                   std::string socket = "", const Filter &filter = nullptr) {
  if (child.id == parent.id) return false;
  if (filter && !filter(child, parent)) return false;
  // Direct back-edge would cycle; deeper chains need the collection overload.
  if (parent.has<AttachedTo>() && parent.get<AttachedTo>().parent == child.id)
    return false;
  auto &rel = child.addComponentIfMissing<AttachedTo>();
  rel.parent = parent.id;
  rel.socket = std::move(socket);
  rel.local = local;
  return true;
}

inline bool attach(EntityCollection &ec, Entity &child, Entity &parent,
                   Pose2D local = {}, std::string socket = "",
                   const Filter &filter = nullptr) {
  if (child.id == parent.id) return false;
  if (filter && !filter(child, parent)) return false;
  for (EntityID cur = parent.id; cur >= 0;) {
    if (cur == child.id) return false;
    const Entity *e = find(ec, cur);
    if (!e || !e->has<AttachedTo>() || !e->get<AttachedTo>().attached()) break;
    cur = e->get<AttachedTo>().parent;
  }
  return attach(child, parent, local, std::move(socket));
}

inline void detach(Entity &child) { child.removeComponentIfExists<AttachedTo>(); }
inline bool is_attached(const Entity &e) {
  return e.has<AttachedTo>() && e.get<AttachedTo>().attached();
}
inline std::optional<EntityID> parent_of(const Entity &e) {
  if (!is_attached(e)) return std::nullopt;
  return e.get<AttachedTo>().parent;
}
inline std::vector<EntityID> children_of(const EntityCollection &ec, EntityID parent) {
  std::vector<EntityID> out;
  for (auto &sp : ec.get_entities())
    if (sp && is_attached(*sp) && sp->get<AttachedTo>().parent == parent)
      out.push_back(sp->id);
  return out;
}

// World pose: attached entities ignore their own HasPose2D and inherit the
// parent's world pose composed with `local`; unattached ones use HasPose2D.
inline std::optional<Pose2D> world_pose(const EntityCollection &ec,
                                        const Entity &e, int depth = 0) {
  if (depth > 64) return std::nullopt; // cycle guard for hand-edited relations
  if (is_attached(e)) {
    const Entity *p = find(ec, e.get<AttachedTo>().parent);
    if (!p) return std::nullopt;
    auto pw = world_pose(ec, *p, depth + 1);
    if (!pw) return std::nullopt;
    return compose(*pw, e.get<AttachedTo>().local);
  }
  if (e.has<HasPose2D>()) return e.get<HasPose2D>().pose;
  return std::nullopt;
}

// Detach every relation whose parent is gone or marked cleanup, and strip
// relations from entities themselves marked cleanup. Call before
// EntityCollection::cleanup() so survivors never point at a dead parent.
// Returns how many relations were removed.
inline int prune(EntityCollection &ec) {
  int n = 0;
  for (auto &sp : ec.get_entities_for_mod()) {
    if (!sp || !is_attached(*sp)) continue;
    const Entity *p = find(ec, sp->get<AttachedTo>().parent);
    if (sp->cleanup || !p || p->cleanup) { detach(*sp); n++; }
  }
  return n;
}

} // namespace attachment
} // namespace afterhours
