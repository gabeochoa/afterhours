#pragma once

#include <concepts>
#include <string>

namespace afterhours {
namespace text_input {

// Concept for pluggable text storage backends (e.g., gap buffer, rope)
// Allows custom implementations for large text editing (word processors, etc.)
template <typename T>
concept TextStorage = requires(T storage, const T const_storage, size_t pos,
                               const std::string &str) {
  // Get text content for display
  { const_storage.str() } -> std::convertible_to<std::string>;
  // Get size in bytes
  { const_storage.size() } -> std::convertible_to<size_t>;
  // Insert string at position
  { storage.insert(pos, str) } -> std::same_as<void>;
  // Erase n bytes starting at position
  { storage.erase(pos, size_t{}) } -> std::same_as<void>;
  // Clear all content
  { storage.clear() } -> std::same_as<void>;
};

// Default std::string-based storage that satisfies TextStorage concept
struct StringStorage {
  std::string data;

  StringStorage() = default;
  explicit StringStorage(const std::string &s) : data(s) {}

  std::string str() const { return data; }
  size_t size() const { return data.size(); }
  void insert(size_t pos, const std::string &s) { data.insert(pos, s); }
  void erase(size_t pos, size_t len) { data.erase(pos, len); }
  void clear() { data.clear(); }
};

} // namespace text_input
} // namespace afterhours

