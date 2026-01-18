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
