#pragma once

#include <memory>
#include <type_traits>
#include <vector>

#include "../logging.h"
#include "../type_name.h"

namespace afterhours {

struct BaseComponent;

#if !defined(AFTER_HOURS_MAX_COMPONENTS)
#define AFTER_HOURS_MAX_COMPONENTS 128
#endif

constexpr size_t max_num_components = AFTER_HOURS_MAX_COMPONENTS;

using ComponentID = size_t;

namespace components {
namespace internal {
inline ComponentID get_unique_id() noexcept {
  static ComponentID lastID{0};
  if (lastID >= max_num_components) {
    log_error(
        "You are trying to add a new component but you have used up all "
        "the space allocated (max: %zu), increase AFTER_HOURS_MAX_COMPONENTS",
        max_num_components);
    return max_num_components - 1;
  }
  return lastID++;
}

} // namespace internal

template <typename T> inline ComponentID get_type_id() noexcept {
  static_assert(std::is_base_of<BaseComponent, T>::value,
                "T must inherit from BaseComponent");
  static ComponentID typeID{internal::get_unique_id()};
  return typeID;
}
} // namespace components

struct BaseComponent {
  BaseComponent() {}
  BaseComponent(BaseComponent &&) = default;
  virtual ~BaseComponent() {}
};
} // namespace afterhours
