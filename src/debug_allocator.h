#pragma once

#include <atomic>
#include <cstddef>
#include <vector>

#include "logging.h"

namespace afterhours {
namespace developer {

// Process-wide DebugAllocator tally, shared across every T via these globals.
struct AllocCounters {
  std::size_t allocations = 0;       // total allocate() calls
  std::size_t live_allocations = 0;  // allocate()s not yet deallocated
  std::size_t live_bytes = 0;        // bytes currently checked out
  std::size_t total_bytes = 0;       // bytes ever allocated
};

namespace detail {
inline std::atomic<std::size_t> allocations{0};
inline std::atomic<std::size_t> live_allocations{0};
inline std::atomic<std::size_t> live_bytes{0};
inline std::atomic<std::size_t> total_bytes{0};
} // namespace detail

inline AllocCounters allocation_counters() {
  return {detail::allocations.load(std::memory_order_relaxed),
          detail::live_allocations.load(std::memory_order_relaxed),
          detail::live_bytes.load(std::memory_order_relaxed),
          detail::total_bytes.load(std::memory_order_relaxed)};
}

inline void reset_allocation_counters() {
  detail::allocations.store(0, std::memory_order_relaxed);
  detail::live_allocations.store(0, std::memory_order_relaxed);
  detail::live_bytes.store(0, std::memory_order_relaxed);
  detail::total_bytes.store(0, std::memory_order_relaxed);
}

// Feed the current tally into a profiling Collector's counters. Templated so
// this header stays free of the profiling include (which cycles via system.h).
template <typename Collector> bool feed_alloc_counters(Collector &collector) {
  const auto snapshot = allocation_counters();
  bool ok = collector.counter("alloc.count", "allocations",
                              static_cast<double>(snapshot.allocations));
  ok = collector.counter("alloc.live_bytes", "bytes", static_cast<double>(snapshot.live_bytes)) &&
       ok;
  ok = collector.counter("alloc.total_bytes", "bytes", static_cast<double>(snapshot.total_bytes)) &&
       ok;
  return ok;
}

template <typename T> class DebugAllocator : public std::allocator<T> {
public:
  using value_type = T;

  constexpr static std::string_view red = "\033[31m";
  constexpr static std::string_view green = "\033[32m";
  constexpr static std::string_view clear = "\033[0m";

  DebugAllocator() = default;

  template <typename U> DebugAllocator(const DebugAllocator<U> &) {}

  T *allocate(const std::size_t n) {
    const auto bytes = n * sizeof(T);
    detail::allocations.fetch_add(1, std::memory_order_relaxed);
    detail::live_allocations.fetch_add(1, std::memory_order_relaxed);
    detail::live_bytes.fetch_add(bytes, std::memory_order_relaxed);
    detail::total_bytes.fetch_add(bytes, std::memory_order_relaxed);
    log_warn("{}Allocating {} ({} elements of size {}) {}", green, bytes, n, sizeof(T), clear);
    return std::allocator<T>().allocate(n);
  }

  void deallocate(T *p, const std::size_t n) {
    const auto bytes = n * sizeof(T);
    detail::live_allocations.fetch_sub(1, std::memory_order_relaxed);
    detail::live_bytes.fetch_sub(bytes, std::memory_order_relaxed);
    log_warn("{}Deallocating {} ({} elements of size {}) {}", red, bytes, n, sizeof(T), clear);
    std::allocator<T>().deallocate(p, n);
  }
};

} // namespace developer
} // namespace afterhours
