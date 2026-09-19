#pragma once

#include "../../../expected.hpp"
#include <charconv>
#include <cmath>
#include <concepts>
#include <span>
#include <string>

namespace afterhours::terminal {

enum class ArgumentError { Missing, Invalid };

template <typename T>
concept NumericArgument = (std::integral<T> && !std::same_as<T, bool>) ||
                          std::floating_point<T>;

class Arguments {
 public:
  explicit Arguments(std::span<const std::string> words = {}) : words_(words) {}

  size_t size() const { return words_.size(); }
  bool empty() const { return words_.empty(); }
  const std::string &operator[](size_t index) const { return words_[index]; }
  const std::string &front() const { return words_.front(); }
  auto begin() const { return words_.begin(); }
  auto end() const { return words_.end(); }
  Arguments subspan(size_t offset) const { return Arguments(words_.subspan(offset)); }

  template <NumericArgument T>
  tl::expected<T, ArgumentError> get(size_t index) const {
    if (index >= size()) return tl::unexpected(ArgumentError::Missing);
    const auto &word = words_[index];
    T value{};
    const auto *end = word.data() + word.size();
    const auto parsed = std::from_chars(word.data(), end, value);
    if (parsed.ec != std::errc{} || parsed.ptr != end)
      return tl::unexpected(ArgumentError::Invalid);
    if constexpr (std::floating_point<T>) {
      if (!std::isfinite(value)) return tl::unexpected(ArgumentError::Invalid);
    }
    return value;
  }

  template <NumericArgument T>
  tl::expected<T, ArgumentError> get(size_t index, T fallback) const {
    if (index >= size()) return fallback;
    return get<T>(index);
  }

 private:
  std::span<const std::string> words_;
};

}
