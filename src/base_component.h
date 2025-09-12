
#pragma once

#include <memory>
#include <mutex>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include "logging.h"
#include "type_name.h"

namespace afterhours {

struct BaseComponent;

#if !defined(AFTER_HOURS_MAX_COMPONENTS)
#define AFTER_HOURS_MAX_COMPONENTS 128
#endif

constexpr size_t max_num_components = AFTER_HOURS_MAX_COMPONENTS;

using ComponentID = size_t;

namespace components {
namespace internal {
inline std::unordered_map<std::type_index, ComponentID> &registry() {
  static std::unordered_map<std::type_index, ComponentID> type_to_id;
  return type_to_id;
}

inline std::mutex &registry_mutex() {
  static std::mutex mtx;
  return mtx;
}

inline ComponentID &last_assigned_id() {
  static ComponentID lastID{0};
  return lastID;
}

} // namespace internal

template <typename T> inline ComponentID get_type_id() noexcept {
  static_assert(std::is_base_of<BaseComponent, T>::value,
                "T must inherit from BaseComponent");

  const std::type_index idx(typeid(T));
  auto &map = internal::registry();
  auto &mtx = internal::registry_mutex();

  {
    std::lock_guard<std::mutex> lock(mtx);
    const auto it = map.find(idx);
    if (it != map.end()) {
      return it->second;
    }

    ComponentID &lastID = internal::last_assigned_id();
    if (lastID >= max_num_components) {
      log_error(
          "You are trying to add a new component but you have used up all the "
          "space allocated (max: %zu), increase AFTER_HOURS_MAX_COMPONENTS",
          max_num_components);
      return max_num_components - 1;
    }

    const ComponentID newId = lastID++;
    map.emplace(idx, newId);
    return newId;
  }
}
} // namespace components

struct BaseComponent {
  BaseComponent() {}
  BaseComponent(BaseComponent &&) = default;
  virtual ~BaseComponent() {}
};
} // namespace afterhours
