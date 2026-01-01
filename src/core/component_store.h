#pragma once

#include <memory>
#include <type_traits>
#include <vector>

#include "base_component.h"
#include "component_pool.h"

namespace afterhours {

struct Entity;
struct ComponentStore;

// Legacy/global accessors (defined in ecs_world.h).
ComponentStore &global_component_store();

struct ComponentStore {
  // `Entity` is allowed to use the internal RTTI/derived access path.
  friend struct Entity;

  // Legacy/global access path (process-default world).
  // Multi-world callers should prefer owning a ComponentStore per world and
  // routing Entity operations through that world.
  static ComponentStore &get() { return global_component_store(); }

  struct IPool {
    virtual ~IPool() = default;
    virtual void remove(EntityID id) = 0;
    virtual void clear() = 0;
    virtual void flush_end_of_frame() = 0;
    virtual BaseComponent *try_get_base(EntityID id) = 0;
    virtual const BaseComponent *try_get_base(EntityID id) const = 0;
  };

  template <typename T> struct Pool final : IPool {
    ComponentPool<T> pool;
    void remove(EntityID id) override { pool.remove(id); }
    void clear() override { pool.clear(); }
    void flush_end_of_frame() override { pool.flush_end_of_frame(); }
    BaseComponent *try_get_base(EntityID id) override {
      return pool.try_get_base(id);
    }
    const BaseComponent *try_get_base(EntityID id) const override {
      return pool.try_get_base(id);
    }
  };

  template <typename T> ComponentPool<T> &pool_for() {
    const ComponentID cid = components::get_type_id<T>();
    ensure_pool_slot(cid);
    if (!pools_[cid]) {
      pools_[cid] = std::make_unique<Pool<T>>();
    }
    return static_cast<Pool<T> *>(pools_[cid].get())->pool;
  }

  template <typename T, typename... Args>
  T &emplace_for(EntityID id, Args &&...args) {
    return pool_for<T>().emplace(id, std::forward<Args>(args)...);
  }

  template <typename T> void remove_for(EntityID id) {
    const ComponentID cid = components::get_type_id<T>();
    if (cid < pools_.size() && pools_[cid]) {
      pools_[cid]->remove(id);
    }
  }

  template <typename T> [[nodiscard]] T &get_for(EntityID id) {
    return pool_for<T>().get(id);
  }

  template <typename T> [[nodiscard]] const T &get_for(EntityID id) const {
    // Allow const access while still lazily creating pools on demand.
    return const_cast<ComponentStore *>(this)->pool_for<T>().get(id);
  }

  void remove_by_component_id(const ComponentID cid, const EntityID id) {
    if (cid < pools_.size() && pools_[cid]) {
      pools_[cid]->remove(id);
    }
  }

  void clear_all() {
    for (auto &p : pools_) {
      if (p)
        p->clear();
    }
  }

  void flush_end_of_frame() {
    for (auto &p : pools_) {
      if (p)
        p->flush_end_of_frame();
    }
  }

private:
  [[nodiscard]] BaseComponent *try_get_base(const ComponentID cid,
                                           const EntityID id) {
    if (cid >= pools_.size() || !pools_[cid])
      return nullptr;
    return pools_[cid]->try_get_base(id);
  }

  [[nodiscard]] const BaseComponent *try_get_base(const ComponentID cid,
                                                 const EntityID id) const {
    if (cid >= pools_.size() || !pools_[cid])
      return nullptr;
    return pools_[cid]->try_get_base(id);
  }

  void ensure_pool_slot(const ComponentID cid) {
    if (pools_.size() <= cid) {
      pools_.resize(cid + 1);
    }
  }

  std::vector<std::unique_ptr<IPool>> pools_;
};

} // namespace afterhours

