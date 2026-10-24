// E2E Testing Framework - Visible Text Registry
// Track rendered text for assertions
#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
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
    entries_.clear();
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
    entries_.push_back(TextEntry{text});
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
    const bool fully = (left == x && top == y && right == x + w && bottom == y + h);
    entries_.push_back(TextEntry{text, x, y, w, h, fully});
  }

  bool contains_fully_visible(const std::string &needle) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto &e : entries_) {
      if (e.fully_visible && e.text.find(needle) != std::string::npos) return true;
    }
    return contains_joined(needle, /*fully_only=*/true,
                           [](const std::string &s, const std::string &n) {
                             return s.find(n) != std::string::npos;
                           });
  }

  bool contains(const std::string &needle) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto &e : entries_) {
      if (e.text.find(needle) != std::string::npos) return true;
    }
    return contains_joined(needle, /*fully_only=*/false,
                           [](const std::string &s, const std::string &n) {
                             return s.find(n) != std::string::npos;
                           });
  }

  // Case-insensitive substring. Game text is routinely styled to a different
  // case than the string in the source, so a test asserting the source spelling
  // fails for a reason that has nothing to do with what it is testing.
  bool contains_ignoring_case(const std::string &needle) const {
    const std::string want = fold(needle);
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto &e : entries_) {
      if (fold(e.text).find(want) != std::string::npos) return true;
    }
    return contains_joined(want, /*fully_only=*/false,
                           [](const std::string &s, const std::string &n) {
                             return fold(s).find(n) != std::string::npos;
                           });
  }

  bool has_exact(const std::string &needle) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto &e : entries_) {
      if (e.text == needle) return true;
    }
    return false;
  }

  std::string get_all() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string result;
    for (const auto &e : entries_) {
      if (!result.empty())
        result += " | ";
      result += e.text;
    }
    return result;
  }

  std::vector<std::string> get_texts() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> out;
    out.reserve(entries_.size());
    for (const auto &e : entries_) out.push_back(e.text);
    return out;
  }

private:
  struct TextEntry {
    std::string text;
    float x = 0.f, y = 0.f, w = 0.f, h = 0.f;  // 0x0 = position unknown
    bool fully_visible = true;
  };

  static std::string fold(std::string v) {
    for (char &c : v)
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return v;
  }

  // Two consecutive entries join when they sit on the same line and the
  // second starts where the first ends (within one layout gap): the
  // filename + path case, two labels on one row. Entries without a rect
  // (typed text, position unknown) never join, and entries on different
  // lines or far apart stay separate, so unrelated labels cannot fuse
  // into a false match.
  static bool joins_with(const TextEntry &a, const TextEntry &b,
                         std::string &joined) {
    if (a.w <= 0.f || a.h <= 0.f || b.w <= 0.f || b.h <= 0.f) return false;
    if (std::abs(a.y - b.y) > 2.f) return false;
    const float gap = b.x - (a.x + a.w);
    if (gap < -2.f || gap > 32.f) return false;
    joined = a.text + (gap > 0.5f ? " " : "") + b.text;
    return true;
  }

  template <typename Match>
  bool contains_joined(const std::string &needle, bool fully_only,
                       Match match) const {
    for (size_t i = 0; i + 1 < entries_.size(); i++) {
      const auto &a = entries_[i];
      const auto &b = entries_[i + 1];
      if (fully_only && (!a.fully_visible || !b.fully_visible)) continue;
      std::string joined;
      if (joins_with(a, b, joined) && match(joined, needle)) return true;
    }
    return false;
  }

  VisibleTextRegistry() = default;
  mutable std::mutex mutex_;
  std::vector<TextEntry> entries_;
  size_t generation_ = 0;
};

} // namespace testing
} // namespace afterhours
