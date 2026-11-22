#pragma once
#include <algorithm>
#include <functional>
#include <span>
#include <utility>
#include <vector>

#include "../core/entity_collection.h"
#include "../developer.h"

namespace afterhours {

struct ProximityPool {
  using Factory = std::function<void(Entity &)>;
  using Acquire = std::function<void(Entity &, const Vector3Type &, size_t)>;
  using Release = std::function<void(Entity &)>;

  ProximityPool() = default;
  ProximityPool(EntityCollection &collection, size_t capacity,
                Factory factory = {}) {
    init(collection, capacity, std::move(factory));
  }

  void init(EntityCollection &collection, size_t capacity,
            Factory factory = {}) {
    slots_.clear();
    slots_.reserve(capacity);
    for (size_t i = 0; i < capacity; ++i) {
      Entity &e = collection.createEntity();
      if (factory)
        factory(e);
      slots_.push_back(Slot{&e, -1, Vector3Type{}});
    }
  }

  void set_on_acquire(Acquire cb) { on_acquire_ = std::move(cb); }
  void set_on_release(Release cb) { on_release_ = std::move(cb); }

  size_t size() const { return slots_.size(); }
  size_t active_count() const {
    size_t n = 0;
    for (const auto &s : slots_)
      if (s.desired >= 0)
        ++n;
    return n;
  }
  Entity *entity_at(size_t slot) const {
    return slot < slots_.size() ? slots_[slot].entity : nullptr;
  }

  int desired_at(size_t slot) const {
    return slot < slots_.size() ? slots_[slot].desired : -1;
  }

  const Vector3Type *position_at(size_t slot) const {
    if (slot >= slots_.size() || slots_[slot].desired < 0)
      return nullptr;
    return &slots_[slot].position;
  }

  void update(const Vector3Type &player,
              std::span<const Vector3Type> desired) {
    const size_t want = std::min(slots_.size(), desired.size());
    std::vector<size_t> targets(desired.size());
    for (size_t i = 0; i < desired.size(); ++i)
      targets[i] = i;
    std::partial_sort(targets.begin(), targets.begin() + want,
                      targets.end(), [&](size_t a, size_t b) {
                        return dist_sq(desired[a], player) <
                               dist_sq(desired[b], player);
                      });
    targets.resize(want);
    struct Pair {
      float dist;
      size_t slot, target;
    };
    std::vector<Pair> pairs;
    pairs.reserve(slots_.size() * targets.size());
    for (size_t s = 0; s < slots_.size(); ++s)
      for (size_t t : targets)
        pairs.push_back(Pair{dist_sq(slots_[s].position, desired[t]), s, t});
    std::sort(pairs.begin(), pairs.end(),
              [](const Pair &a, const Pair &b) { return a.dist < b.dist; });

    std::vector<int> next(slots_.size(), -1);
    std::vector<char> used(targets.size(), 0);
    std::vector<char> taken(slots_.size(), 0);
    for (const auto &p : pairs) {
      if (taken[p.slot])
        continue;
      const size_t ti = index_of(targets, p.target);
      if (used[ti])
        continue;
      taken[p.slot] = 1;
      used[ti] = 1;
      next[p.slot] = static_cast<int>(p.target);
    }

    for (size_t s = 0; s < slots_.size(); ++s) {
      Slot &slot = slots_[s];
      const int fresh = next[s];
      if (fresh == slot.desired)
        continue;
      if (slot.desired >= 0 && on_release_)
        on_release_(*slot.entity);
      slot.desired = fresh;
      if (fresh >= 0) {
        slot.position = desired[static_cast<size_t>(fresh)];
        if (on_acquire_)
          on_acquire_(*slot.entity, slot.position,
                      static_cast<size_t>(fresh));
      }
    }
  }

private:
  struct Slot {
    Entity *entity;
    int desired = -1;
    Vector3Type position{};
  };

  std::vector<Slot> slots_;
  Acquire on_acquire_;
  Release on_release_;

  static float dist_sq(const Vector3Type &a, const Vector3Type &b) {
    const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
  }
  static size_t index_of(const std::vector<size_t> &v, size_t x) {
    for (size_t i = 0; i < v.size(); ++i)
      if (v[i] == x)
        return i;
    return v.size() - 1;
  }
};

}
