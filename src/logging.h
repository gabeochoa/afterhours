
#pragma once

#include <cstdio>

// Opt-in: skip <format> (the single heaviest include in the ECS core path, ~0.4s
// to parse per TU) for builds that don't need formatted log output. Log calls
// become no-ops. Default keeps the full std::format API.
#if defined(AFTER_HOURS_LEAN_LOGGING) && !defined(AFTER_HOURS_REPLACE_LOGGING)

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

enum {
  VENDOR_LOG_TRACE = 1,
  VENDOR_LOG_INFO = 2,
  VENDOR_LOG_WARN = 3,
  VENDOR_LOG_ERROR = 4
};

#if !defined(AFTER_HOURS_REPLACE_VALIDATE)
inline void VALIDATE(...) {}
#endif
