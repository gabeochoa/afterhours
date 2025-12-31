#pragma once

// Minimal tl::expected / tl::unexpected implementation.
//
// This exists as a fallback for toolchains where <expected> is not available or
// does not expose the feature test macro used by this repo.
//
// It implements only the subset currently needed by `src/library.h`.

#include <type_traits>
#include <utility>

namespace tl {

template <typename E> struct unexpected {
  E error;
};

template <typename E> unexpected(E) -> unexpected<E>;

template <typename T, typename E> class expected {
  static_assert(!std::is_reference_v<T>, "expected<T,E> does not support T&");
  static_assert(!std::is_reference_v<E>, "expected<T,E> does not support E&");

public:
  using value_type = T;
  using error_type = E;

  expected(const T &v) : has_(true) { new (&storage_.value) T(v); }
  expected(T &&v) : has_(true) { new (&storage_.value) T(std::move(v)); }

  // Converting value constructor (enables `return "foo"` when T=std::string).
  template <typename U,
            typename = std::enable_if_t<
                std::is_constructible_v<T, U> &&
                !std::is_same_v<std::remove_cvref_t<U>, expected> &&
                !std::is_same_v<std::remove_cvref_t<U>, unexpected<E>>>>
  expected(U &&u) : has_(true) {
    new (&storage_.value) T(std::forward<U>(u));
  }

  expected(const unexpected<E> &u) : has_(false) {
    new (&storage_.error) E(u.error);
  }
  expected(unexpected<E> &&u) : has_(false) {
    new (&storage_.error) E(std::move(u.error));
  }

  expected(const expected &other) : has_(other.has_) {
    if (has_) {
      new (&storage_.value) T(other.storage_.value);
    } else {
      new (&storage_.error) E(other.storage_.error);
    }
  }

  expected(expected &&other) noexcept(std::is_nothrow_move_constructible_v<T> &&
                                      std::is_nothrow_move_constructible_v<E>)
      : has_(other.has_) {
    if (has_) {
      new (&storage_.value) T(std::move(other.storage_.value));
    } else {
      new (&storage_.error) E(std::move(other.storage_.error));
    }
  }

  expected &operator=(const expected &other) {
    if (this == &other)
      return *this;
    this->~expected();
    new (this) expected(other);
    return *this;
  }

  expected &operator=(expected &&other) noexcept(
      std::is_nothrow_move_constructible_v<T> &&
      std::is_nothrow_move_constructible_v<E>) {
    if (this == &other)
      return *this;
    this->~expected();
    new (this) expected(std::move(other));
    return *this;
  }

  ~expected() {
    if (has_) {
      storage_.value.~T();
    } else {
      storage_.error.~E();
    }
  }

  [[nodiscard]] bool has_value() const { return has_; }
  [[nodiscard]] explicit operator bool() const { return has_; }

  [[nodiscard]] T &operator*() & { return storage_.value; }
  [[nodiscard]] const T &operator*() const & { return storage_.value; }
  [[nodiscard]] T &&operator*() && { return std::move(storage_.value); }

  [[nodiscard]] T *operator->() { return &storage_.value; }
  [[nodiscard]] const T *operator->() const { return &storage_.value; }

  [[nodiscard]] T &value() & { return storage_.value; }
  [[nodiscard]] const T &value() const & { return storage_.value; }
  [[nodiscard]] T &&value() && { return std::move(storage_.value); }

  [[nodiscard]] E &error() & { return storage_.error; }
  [[nodiscard]] const E &error() const & { return storage_.error; }
  [[nodiscard]] E &&error() && { return std::move(storage_.error); }

private:
  bool has_;
  union Storage {
    Storage() {}
    ~Storage() {}
    T value;
    E error;
  } storage_{};
};

} // namespace tl

