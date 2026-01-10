#pragma once

#include <functional>
#include <memory>
#include <type_traits>

namespace afterhours {

// Pointer-like policy helpers for snapshot/serialization surfaces.
//
// Phase 3 goal:
// - Persisted data must not contain raw pointers, smart pointers, or
//   reference-wrapper based entity references.
//
// NOTE: This intentionally operates at the TYPE level (T itself), not by
// recursively introspecting member fields. Use this as a guardrail for
// snapshot/serialization APIs that take component types as template params.

template <typename T> struct is_std_unique_ptr : std::false_type {};
template <typename T, typename D>
struct is_std_unique_ptr<std::unique_ptr<T, D>> : std::true_type {};

template <typename T> struct is_std_shared_ptr : std::false_type {};
template <typename T>
struct is_std_shared_ptr<std::shared_ptr<T>> : std::true_type {};

template <typename T> struct is_std_weak_ptr : std::false_type {};
template <typename T>
struct is_std_weak_ptr<std::weak_ptr<T>> : std::true_type {};

template <typename T> struct is_std_reference_wrapper : std::false_type {};
template <typename T>
struct is_std_reference_wrapper<std::reference_wrapper<T>> : std::true_type {};

template <typename T>
struct is_pointer_like
    : std::bool_constant<
          std::is_pointer_v<std::remove_cvref_t<T>> ||
          is_std_unique_ptr<std::remove_cvref_t<T>>::value ||
          is_std_shared_ptr<std::remove_cvref_t<T>>::value ||
          is_std_weak_ptr<std::remove_cvref_t<T>>::value ||
          is_std_reference_wrapper<std::remove_cvref_t<T>>::value> {};

template <typename T>
inline constexpr bool is_pointer_like_v = is_pointer_like<T>::value;

template <typename... Ts> consteval void static_assert_pointer_free_types() {
  static_assert(((!is_pointer_like_v<Ts>) && ...),
                "Pointer-like types (raw/smart pointers, reference_wrappers, "
                "etc.) are not allowed in pointer-free snapshot/serialization "
                "surfaces. Store EntityHandle/EntityID instead and resolve "
                "through EntityHelper at runtime.");
}

template <typename T> consteval void static_assert_pointer_free_type() {
  static_assert_pointer_free_types<T>();
}

} // namespace afterhours
