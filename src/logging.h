
#pragma once

#include <cstdio>
#include <set>
#include <utility>
#include <version> // __cpp_lib_format / __has_include, without pulling <format>

// Decide whether the full std::format-based logger is available/wanted.
//
// Lean logging (no-op log calls, skips the <format> include — the single
// heaviest include in the ECS core path, ~0.4s to parse per TU) kicks in when:
//   * AFTER_HOURS_LEAN_LOGGING is set explicitly (opt-in), OR
//   * <format> isn't actually available. std::format landed in libstdc++ 13 /
//     libc++ 17, so a C++20-capable-but-older toolchain (e.g. gcc 11/12, the
//     default on many machines) has no <format>. Rather than fail to compile,
//     fall back to no-op logging so afterhours builds out of the box. Log
//     output isn't part of the library's behavioral contract.
// Define AFTER_HOURS_REQUIRE_FORMAT to turn a missing <format> back into a hard
// error instead of silently going lean.
#if defined(__cpp_lib_format) && __has_include(<format>)
#define AFTER_HOURS_HAS_FORMAT 1
#else
#define AFTER_HOURS_HAS_FORMAT 0
#endif

#if !AFTER_HOURS_HAS_FORMAT && defined(AFTER_HOURS_REQUIRE_FORMAT)
#error "afterhours: <format> not available (need libstdc++ 13+/libc++ 17+ or a newer compiler). Unset AFTER_HOURS_REQUIRE_FORMAT to fall back to no-op logging."
#endif

#if (defined(AFTER_HOURS_LEAN_LOGGING) || !AFTER_HOURS_HAS_FORMAT) &&           \
    !defined(AFTER_HOURS_REPLACE_LOGGING)

template <typename... Args> inline void log_trace(const char *, Args &&...) {}
template <typename... Args> inline void log_info(const char *, Args &&...) {}
template <typename... Args> inline void log_warn(const char *, Args &&...) {}
template <typename... Args> inline void log_error(const char *, Args &&...) {}
template <typename... Args> inline void log_clean(const char *, Args &&...) {}
template <typename Duration, typename... Args>
inline void log_once_per(Duration, int, const char *, Args &&...) {}

#elif !defined(AFTER_HOURS_REPLACE_LOGGING)

#include <format>

// C++20 format-based logging with {} placeholders

template <typename... Args>
inline void log_trace(std::format_string<Args...>, Args &&...) {
  // For now, trace logging is disabled
}

template <typename... Args>
inline void log_info(std::format_string<Args...> fmt, Args &&...args) {
  std::fprintf(stdout, "[INFO] %s\n",
               std::format(fmt, std::forward<Args>(args)...).c_str());
}

template <typename... Args>
inline void log_warn(std::format_string<Args...> fmt, Args &&...args) {
  std::fprintf(stdout, "[WARN] %s\n",
               std::format(fmt, std::forward<Args>(args)...).c_str());
}

template <typename... Args>
inline void log_error(std::format_string<Args...> fmt, Args &&...args) {
  std::fprintf(stderr, "[ERROR] %s\n",
               std::format(fmt, std::forward<Args>(args)...).c_str());
}

template <typename... Args>
inline void log_clean(std::format_string<Args...>, Args &&...) {
  // For now, clean logging is disabled
}

template <typename Duration, typename... Args>
inline void log_once_per(Duration, int, std::format_string<Args...>,
                         Args &&...) {
  // For now, once per logging is disabled
}

#endif

// A diagnostic that fires from a per-frame path has to be gated or it prints
// every frame forever. Deliberately outside the mode blocks above: it expands
// to whichever log_warn is in scope at the call site, including the one a test
// substitutes via AFTER_HOURS_REPLACE_LOGGING.
//
//   warn_once(entity.id, "'{}' will never fire: ...", name);
//   warn_once(font_key, "No font registered for '{}'", font_key);
//
// `key` is what makes two reports distinct -- an entity id, a font name, a
// value. Each call site gets its own gate, so the same key at two sites does
// not silence one of them.
namespace log_detail {
template <typename Key> inline bool warn_gate(const void *site, Key key) {
  static std::set<std::pair<const void *, Key>> seen;
  return seen.insert({site, std::move(key)}).second;
}
} // namespace log_detail

#define warn_once(key, ...)                                                    \
  do {                                                                         \
    /* Address is unique per expansion, which is what scopes the gate. */      \
    static const char afterhours_warn_site = 0;                                \
    if (::log_detail::warn_gate(&afterhours_warn_site, (key)))                 \
      log_warn(__VA_ARGS__);                                                   \
  } while (0)

enum {
  VENDOR_LOG_TRACE = 1,
  VENDOR_LOG_INFO = 2,
  VENDOR_LOG_WARN = 3,
  VENDOR_LOG_ERROR = 4
};

#if !defined(AFTER_HOURS_REPLACE_VALIDATE)
inline void VALIDATE(...) {}
#endif
