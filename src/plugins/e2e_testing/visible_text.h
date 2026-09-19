// E2E Testing Framework - Visible Text Registry
// Track rendered text for assertions
#pragma once

#include <algorithm>
#include <cctype>
#include <mutex>
#include <string>
#include <vector>

#include "../../logging.h"

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
    fully_visible_texts_.clear();
    generation_++;
  }

  /// Bumped by clear(), which runs once per render pass. An assertion that
  /// needs fresh data waits for this to change rather than counting frames:
  /// an app is free to tick many times per rendered frame, and only a render
  /// refills the registry.
  size_t generation() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return generation_;
  }

  void register_text(const std::string &text) {
    if (text.empty())
      return;
    std::lock_guard<std::mutex> lock(mutex_);
    texts_.push_back(text);
    fully_visible_texts_.push_back(text);
  }

  /// Register text only if its bounding rect is at least partially visible
  /// within the viewport. Off-screen text is silently skipped.
  void register_text_if_visible(const std::string &text, float rect_x,
                                float rect_y, float rect_w, float rect_h,
                                float viewport_w, float viewport_h) {
    register_text_in_clip(text, rect_x, rect_y, rect_w, rect_h,
                          0.f, 0.f, viewport_w, viewport_h);
  }

  void register_text_in_clip(const std::string &text, float x, float y,
                             float w, float h, float clip_x, float clip_y,
                             float clip_w, float clip_h) {
    if (text.empty() || w <= 0.f || h <= 0.f ||
        clip_w <= 0.f || clip_h <= 0.f) return;
    const float left = std::max(x, clip_x);
    const float top = std::max(y, clip_y);
    const float right = std::min(x + w, clip_x + clip_w);
    const float bottom = std::min(y + h, clip_y + clip_h);
    if (right - left < 1.f || bottom - top < 1.f) return;
    std::lock_guard<std::mutex> lock(mutex_);
    texts_.push_back(text);
    if (left == x && top == y && right == x + w && bottom == y + h)
      fully_visible_texts_.push_back(text);
  }

  bool contains_fully_visible(const std::string &needle) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto &text : fully_visible_texts_) {
      if (text.find(needle) != std::string::npos) return true;
    }
    return false;
  }

  bool contains(const std::string &needle) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto &t : texts_) {
      if (t.find(needle) != std::string::npos)
        return true;
    }
    return false;
  }

  // Case-insensitive substring. Game text is routinely styled to a different
  // case than the string in the source, so a test asserting the source spelling
  // fails for a reason that has nothing to do with what it is testing.
  bool contains_ignoring_case(const std::string &needle) const {
    const auto fold = [](std::string v) {
      for (char &c : v)
        c = static_cast<char>(
            std::tolower(static_cast<unsigned char>(c)));
      return v;
    };
    const std::string want = fold(needle);
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto &t : texts_) {
      if (fold(t).find(want) != std::string::npos)
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
  std::vector<std::string> fully_visible_texts_;
  size_t generation_ = 0;
};

} // namespace testing
} // namespace afterhours
