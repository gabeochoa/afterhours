#pragma once

#include <string>
#include <string_view>
#include <cstdint>
#include <utility>

namespace afterhours {
namespace clipboard {

struct Provider {
  virtual ~Provider() = default;
  virtual void set_text(std::string_view text) = 0;
  virtual std::string get_text() const = 0;
};

class MemoryProvider final : public Provider {
 public:
  void set_text(std::string_view text) override {
    text_ = text;
    ++generation_;
  }
  std::string get_text() const override { return text_; }
  std::uint64_t generation() const { return generation_; }
  void reset() { text_.clear(); generation_ = 0; }

 private:
  std::string text_;
  std::uint64_t generation_ = 0;
};

namespace detail {
inline thread_local Provider *provider = nullptr;

#ifdef AFTER_HOURS_USE_RAYLIB

// Set the system clipboard to the specified UTF-8 text
inline void set_text(std::string_view text) {
  // raylib's SetClipboardText expects a null-terminated C string
  std::string str(text);
  raylib::SetClipboardText(str.c_str());
}

// Get the current clipboard contents as a UTF-8 string
// Returns empty string if clipboard is empty or doesn't contain text
inline std::string get_text() {
  const char *text = raylib::GetClipboardText();
  if (text == nullptr) {
    return "";
  }
  return std::string(text);
}

// Check if the clipboard contains text
inline bool has_text() {
  const char *text = raylib::GetClipboardText();
  return text != nullptr && text[0] != '\0';
}

#elif defined(AFTER_HOURS_USE_METAL)

// Sokol backend — uses sapp clipboard functions.
// Requires desc.enable_clipboard = true in sapp_desc setup.
inline void set_text(std::string_view text) {
  std::string str(text);
  sapp_set_clipboard_string(str.c_str());
}

inline std::string get_text() {
  const char *text = sapp_get_clipboard_string();
  if (text == nullptr) {
    return "";
  }
  return std::string(text);
}

inline bool has_text() {
  const char *text = sapp_get_clipboard_string();
  return text != nullptr && text[0] != '\0';
}

#else

// Fallback implementations when no backend is available
inline void set_text(std::string_view) {}

inline std::string get_text() { return ""; }

inline bool has_text() { return false; }

#endif

}

class ScopedProvider {
 public:
  explicit ScopedProvider(Provider &provider)
      : previous_(std::exchange(detail::provider, &provider)) {}
  ~ScopedProvider() { detail::provider = previous_; }
  ScopedProvider(const ScopedProvider &) = delete;
  ScopedProvider &operator=(const ScopedProvider &) = delete;

 private:
  Provider *previous_;
};

inline void set_text(std::string_view text) {
  if (detail::provider) {
    detail::provider->set_text(text);
    return;
  }
  detail::set_text(text);
}

inline std::string get_text() {
  return detail::provider ? detail::provider->get_text() : detail::get_text();
}

inline bool has_text() {
  return detail::provider ? !detail::provider->get_text().empty() : detail::has_text();
}

} // namespace clipboard
} // namespace afterhours
