// E2E Testing Framework - Visible Text Registry
// Track rendered text for assertions
#pragma once

#include <mutex>
#include <string>
#include <vector>

namespace afterhours {
namespace testing {

class VisibleTextRegistry {
public:
  static VisibleTextRegistry &instance() {
    static VisibleTextRegistry inst;
    return inst;
  }

  void clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    texts_.clear();
  }

  void register_text(const std::string &text) {
    if (text.empty())
      return;
    std::lock_guard<std::mutex> lock(mutex_);
    texts_.push_back(text);
  }

  /// Register text only if its bounding rect is at least partially visible
  /// within the viewport. Off-screen text is silently skipped.
  void register_text_if_visible(const std::string &text, float rect_x,
                                float rect_y, float rect_w, float rect_h,
                                float viewport_w, float viewport_h) {
    if (text.empty())
      return;
    // Must have at least 1px of the rect visible inside the viewport
    if (rect_x + rect_w <= 0 || rect_y + rect_h <= 0 || rect_x >= viewport_w ||
        rect_y >= viewport_h)
      return;
    // Degenerate rects (zero/negative size) are not visible
    if (rect_w < 1.f || rect_h < 1.f)
      return;
    std::lock_guard<std::mutex> lock(mutex_);
    texts_.push_back(text);
  }

  bool contains(const std::string &needle) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto &t : texts_) {
      if (t.find(needle) != std::string::npos)
        return true;
    }
    return false;
  }

  bool has_exact(const std::string &needle) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto &t : texts_) {
      if (t == needle)
        return true;
    }
    return false;
  }

  std::string get_all() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string result;
    for (const auto &t : texts_) {
      if (!result.empty())
        result += " | ";
      result += t;
    }
    return result;
  }

  std::vector<std::string> get_texts() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return texts_;
  }

private:
  VisibleTextRegistry() = default;
  mutable std::mutex mutex_;
  std::vector<std::string> texts_;
};

} // namespace testing
} // namespace afterhours
