
#pragma once

#include <vector>

#include "logging.h"

namespace afterhours {
namespace developer {
template <typename T> class DebugAllocator : public std::allocator<T> {
public:
  using value_type = T;

  constexpr static std::string_view red = "\033[31m";
  constexpr static std::string_view green = "\033[32m";
  constexpr static std::string_view clear = "\033[0m";

  DebugAllocator() = default;

  template <typename U> DebugAllocator(const DebugAllocator<U> &) {}

  T *allocate(const std::size_t n) {
    log_warn("{}Allocating {} ({} elements of size {}) {}", green,
             n * sizeof(T), n, sizeof(T), clear);
    return std::allocator<T>().allocate(n);
  }

  void deallocate(T *p, const std::size_t n) {
    log_warn("{}Deallocating {} ({} elements of size {}) {}", red,
             n * sizeof(T), n, sizeof(T), clear);
    std::allocator<T>().deallocate(p, n);
  }
};

} // namespace developer
} // namespace afterhours
