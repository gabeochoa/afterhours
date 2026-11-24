#pragma once


#include <cmath>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "../core/base_component.h"
#include "../developer.h"
#include "../ecs.h"

namespace afterhours {

struct Pose2D {
  float x = 0.f, y = 0.f, angle_deg = 0.f;
};

struct HasPose2D : BaseComponent {
  Pose2D pose;
  HasPose2D() = default;
  explicit HasPose2D(Pose2D p) : pose(p) {}
  HasPose2D(float x, float y, float a = 0.f) : pose{x, y, a} {}
};

struct AttachedTo : BaseComponent {
  EntityID parent = -1;
  std::string socket;
  Pose2D local;
  [[nodiscard]] bool attached() const { return parent >= 0; }
};

namespace attachment {

using Filter = std::function<bool(const Entity &child, const Entity &parent)>;

inline Pose2D compose(const Pose2D &parent, const Pose2D &local) {
  const float r = parent.angle_deg * DEG2RAD;
  const float c = std::cos(r), s = std::sin(r);
  return {parent.x + local.x * c - local.y * s,
          parent.y + local.x * s + local.y * c, parent.angle_deg + local.angle_deg};
}

inline OptEntity find(EntityCollection &ec, EntityID id) {
  for (auto &sp : ec.get_entities_for_mod())
    if (sp && sp->id == id) return *sp;
  return {};
}
inline OptEntity find(const EntityCollection &ec, EntityID id) {
  for (auto &sp : ec.get_entities())
    if (sp && sp->id == id) return *sp;
  return {};
}

inline bool attach(Entity &child, Entity &parent, Pose2D local = {},
                   std::string socket = "", const Filter &filter = nullptr) {
  if (child.id == parent.id) return false;
  if (filter && !filter(child, parent)) return false;
  if (parent.has<AttachedTo>() && parent.get<AttachedTo>().parent == child.id)
    return false;
  auto &rel = *child.toggleComponent<AttachedTo>(true);
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
    const OptEntity e = find(ec, cur);
    if (!e || !e->has<AttachedTo>() || !e->get<AttachedTo>().attached()) break;
    cur = e->get<AttachedTo>().parent;
  }
  return attach(child, parent, local, std::move(socket));
}

inline void detach(Entity &child) { child.toggleComponent<AttachedTo>(false); }
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

inline std::optional<Pose2D> world_pose(const EntityCollection &ec,
                                        const Entity &e, int depth = 0) {
  if (depth > 64) return std::nullopt;
  if (is_attached(e)) {
    const OptEntity p = find(ec, e.get<AttachedTo>().parent);
    if (!p) return std::nullopt;
    auto pw = world_pose(ec, p.asE(), depth + 1);
    if (!pw) return std::nullopt;
    return compose(*pw, e.get<AttachedTo>().local);
  }
  if (e.has<HasPose2D>()) return e.get<HasPose2D>().pose;
  return std::nullopt;
}

inline int prune(EntityCollection &ec) {
  int n = 0;
  for (auto &sp : ec.get_entities_for_mod()) {
    if (!sp || !is_attached(*sp)) continue;
    const OptEntity p = find(ec, sp->get<AttachedTo>().parent);
    if (sp->cleanup || !p || p->cleanup) { detach(*sp); n++; }
  }
  return n;
}

}
}
