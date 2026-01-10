#pragma once

#include <type_traits>

// For bitwise operations
template <typename T>
constexpr auto operator~(const T a) ->
    typename std::enable_if<std::is_enum<T>::value, T>::type {
  return static_cast<T>(~static_cast<int>(a));
}

template <typename T>
constexpr auto operator|(const T a, const T b) ->
    typename std::enable_if<std::is_enum<T>::value, T>::type {
  return static_cast<T>((static_cast<int>(a) | static_cast<int>(b)));
}

template <typename T>
constexpr auto operator^(const T a, const T b) ->
    typename std::enable_if<std::is_enum<T>::value, T>::type {
  return static_cast<T>((static_cast<int>(a) ^ static_cast<int>(b)));
}

template <typename T>
constexpr auto operator|=(T &a, const T b) ->
    typename std::enable_if<std::is_enum<T>::value, T>::type {
  return reinterpret_cast<T &>(
      (reinterpret_cast<int &>(a) |= static_cast<int>(b)));
}

template <typename T>
constexpr auto operator&=(T &a, const T b) ->
    typename std::enable_if<std::is_enum<T>::value, T>::type {
  return reinterpret_cast<T &>(
      (reinterpret_cast<int &>(a) &= static_cast<int>(b)));
}

template <typename T>
constexpr auto operator^=(T &a, const T b) ->
    typename std::enable_if<std::is_enum<T>::value, T>::type {
  return reinterpret_cast<T &>(
      (reinterpret_cast<int &>(a) ^= static_cast<int>(b)));
}

// Note: we dont use this one, cause we use the & for bool
// template <typename T>
// constexpr auto operator&(T a, T b) ->
// typename std::enable_if<std::is_enum<T>::value, T>::type {
// return static_cast<T>((static_cast<int>(a) & static_cast<int>(b)));
// }

template <typename T,
          typename = typename std::enable_if<std::is_enum<T>::value, T>::type>
class auto_bool {
  T val_;

public:
  constexpr auto_bool(const T val) : val_(val) {}
  constexpr operator T() const { return val_; }
  constexpr explicit operator bool() const {
    return static_cast<std::underlying_type_t<T>>(val_) != 0;
  }
};

template <typename T,
          typename = typename std::enable_if<std::is_enum<T>::value, T>::type>
constexpr auto_bool<T> operator&(const T lhs, const T rhs) {
  return static_cast<T>(
      static_cast<typename std::underlying_type<T>::type>(lhs) &
      static_cast<typename std::underlying_type<T>::type>(rhs));
}
