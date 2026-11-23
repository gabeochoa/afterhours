#pragma once
#include <algorithm>
#include <functional>
#include <numeric>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "../core/entity_collection.h"
#include "../developer.h"

namespace afterhours {

struct ProximityPool {
  using FactoryFn = std::function<void(Entity &)>;
  using AcquireFn =
      std::function<void(Entity &, const Vector3Type &, size_t)>;
  using ReleaseFn = std::function<void(Entity &)>;

  ProximityPool() = default;
  ProximityPool(EntityCollection &collection, size_t capacity,
                FactoryFn factory = {}) {
    init(collection, capacity, std::move(factory));
  }

  void init(EntityCollection &collection, size_t capacity,
            FactoryFn factory = {}) {
    slots.clear();
    slots.reserve(capacity);
    for (size_t i = 0; i < capacity; ++i) {
      Entity &e = collection.createEntity();
      if (factory)
        factory(e);
      slots.push_back(Slot{RefEntity{e}, -1, Vector3Type{}});
    }
  }

  void set_on_acquire(AcquireFn cb) { on_acquire = std::move(cb); }
  void set_on_release(ReleaseFn cb) { on_release = std::move(cb); }

  size_t size() const { return slots.size(); }
  size_t active_count() const {
    return static_cast<size_t>(std::count_if(
        slots.begin(), slots.end(),
        [](const Slot &s) { return s.desired >= 0; }));
  }
  OptEntity entity_at(size_t slot) const {
    return slot < slots.size() ? OptEntity{slots[slot].entity} : OptEntity{};
  }

  int desired_at(size_t slot) const {
    return slot < slots.size() ? slots[slot].desired : -1;
  }

  std::optional<Vector3Type> position_at(size_t slot) const {
    if (slot >= slots.size() || slots[slot].desired < 0)
      return std::nullopt;
    return slots[slot].position;
  }

  void update(const Vector3Type &player,
              std::span<const Vector3Type> desired) {
    const std::vector<size_t> targets = nearest_targets(player, desired);
    const std::vector<int> next = assign_slots(targets, desired);
    apply_assignment(next, desired);
  }

private:
  struct Slot {
    RefEntity entity;
    int desired = -1;
    Vector3Type position{};
  };

  std::vector<Slot> slots;
  AcquireFn on_acquire;
  ReleaseFn on_release;

  std::vector<size_t> nearest_targets(
      const Vector3Type &player,
      std::span<const Vector3Type> desired) const {
    const size_t want = std::min(slots.size(), desired.size());
    std::vector<size_t> targets(desired.size());
    std::iota(targets.begin(), targets.end(), size_t{0});
    std::partial_sort(targets.begin(), targets.begin() + want, targets.end(),
                      [&](size_t a, size_t b) {
                        return distance_sq(desired[a], player) <
                               distance_sq(desired[b], player);
                      });
    targets.resize(want);
    return targets;
  }

  std::vector<int> assign_slots(std::span<const size_t> targets,
                                std::span<const Vector3Type> desired) const {
    struct Pair {
      float dist;
      size_t slot, target;
    };
    std::vector<Pair> pairs;
    pairs.reserve(slots.size() * targets.size());
    for (size_t s = 0; s < slots.size(); ++s)
      for (size_t t : targets)
        pairs.push_back(Pair{distance_sq(slots[s].position, desired[t]), s, t});
    std::sort(pairs.begin(), pairs.end(),
              [](const Pair &a, const Pair &b) { return a.dist < b.dist; });

    std::vector<int> next(slots.size(), -1);
    std::vector<char> used(targets.size(), 0);
    std::vector<char> taken(slots.size(), 0);
    for (const auto &p : pairs) {
      if (taken[p.slot])
        continue;
      const auto it = std::find(targets.begin(), targets.end(), p.target);
      if (it == targets.end())
        continue;
      const size_t ti = static_cast<size_t>(it - targets.begin());
      if (used[ti])
        continue;
      taken[p.slot] = 1;
      used[ti] = 1;
      next[p.slot] = static_cast<int>(p.target);
    }
    return next;
  }

  void apply_assignment(std::span<const int> next,
                        std::span<const Vector3Type> desired) {
    for (size_t s = 0; s < slots.size(); ++s) {
      Slot &slot = slots[s];
      const int fresh = next[s];
      if (fresh == slot.desired)
        continue;
      if (slot.desired >= 0 && on_release)
        on_release(slot.entity.get());
      slot.desired = fresh;
      if (fresh >= 0) {
        slot.position = desired[static_cast<size_t>(fresh)];
        if (on_acquire)
          on_acquire(slot.entity.get(), slot.position,
                     static_cast<size_t>(fresh));
      }
    }
  }
};

}
