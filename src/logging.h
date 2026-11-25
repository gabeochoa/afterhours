
#pragma once

#include <cstdlib>

#include <cstdio>
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

// Somewhere else for the log to go. log_file.h is the one in the box; a
// pointer, not an include, so a TU that does not want it pays nothing.
inline void (*log_sink_fn)(const char *level, const char *message) = nullptr;

namespace afterhours {

enum class LogLevel {
  LOG_TRACE = 0,
  LOG_DEBUG = 1,
  LOG_INFO = 2,
  LOG_WARN = 3,
  LOG_ERROR = 4,
};

} // namespace afterhours

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

#include <chrono>
#include <format>
#include <string>
#include <string_view>
#include <unordered_map>

// C++20 format-based logging with {} placeholders

namespace afterhours {

inline LogLevel &min_log_level_storage() {
  static LogLevel level = LogLevel::LOG_TRACE;
  return level;
}

inline void set_log_level(LogLevel level) { min_log_level_storage() = level; }

inline LogLevel get_log_level() { return min_log_level_storage(); }

inline void log_v(LogLevel level, std::string_view message) {
  if (level < get_log_level())
    return;
  const char *tag = "[INFO]";
  std::FILE *stream = stdout;
  switch (level) {
  case LogLevel::LOG_TRACE:
    tag = "[TRACE]";
    break;
  case LogLevel::LOG_DEBUG:
    tag = "[DEBUG]";
    break;
  case LogLevel::LOG_INFO:
    tag = "[INFO]";
    break;
  case LogLevel::LOG_WARN:
    tag = "[WARN]";
    break;
  case LogLevel::LOG_ERROR:
    tag = "[ERROR]";
    stream = stderr;
    break;
  }
  const std::string text{message};
  std::fprintf(stream, "%s %s\n", tag, text.c_str());
  if (log_sink_fn)
    log_sink_fn(tag, text.c_str());
}

template <typename... Args>
inline void log_v(LogLevel level, std::format_string<Args...> fmt,
                  Args &&...args) {
  if (level < get_log_level())
    return;
  const std::string message = std::format(fmt, std::forward<Args>(args)...);
  log_v(level, std::string_view{message});
}

} // namespace afterhours

template <typename... Args>
inline void log_trace(std::format_string<Args...> fmt, Args &&...args) {
  afterhours::log_v(afterhours::LogLevel::LOG_TRACE, fmt,
                    std::forward<Args>(args)...);
}

template <typename... Args>
inline void log_info(std::format_string<Args...> fmt, Args &&...args) {
  afterhours::log_v(afterhours::LogLevel::LOG_INFO, fmt,
                    std::forward<Args>(args)...);
}

template <typename... Args>
inline void log_warn(std::format_string<Args...> fmt, Args &&...args) {
  afterhours::log_v(afterhours::LogLevel::LOG_WARN, fmt,
                    std::forward<Args>(args)...);
}

template <typename... Args>
inline void log_error(std::format_string<Args...> fmt, Args &&...args) {
  afterhours::log_v(afterhours::LogLevel::LOG_ERROR, fmt,
                    std::forward<Args>(args)...);
}

template <typename... Args>
inline void log_clean(std::format_string<Args...>, Args &&...) {
  // For now, clean logging is disabled
}

template <typename Duration, typename... Args>
inline void log_once_per(Duration interval, int key,
                         std::format_string<Args...> fmt, Args &&...args) {
  static std::unordered_map<int, std::chrono::steady_clock::time_point> last;
  const auto now = std::chrono::steady_clock::now();
  const auto it = last.find(key);
  if (it != last.end() && now - it->second < interval)
    return;
  last[key] = now;
  afterhours::log_v(afterhours::LogLevel::LOG_INFO, fmt,
                    std::forward<Args>(args)...);
}

#endif

enum {
  VENDOR_LOG_TRACE = 1,
  VENDOR_LOG_INFO = 2,
  VENDOR_LOG_WARN = 3,
  VENDOR_LOG_ERROR = 4
};

// A macro, so release does not evaluate the arguments -- sizeof doesn't
// evaluate its operand, and it keeps the compiler quiet about variables only
// used in a check.
#if !defined(AFTER_HOURS_REPLACE_VALIDATE)
#if defined(NDEBUG)
#define VALIDATE(condition, message)                                           \
  ((void)sizeof((condition)), (void)sizeof((message)))
#else
[[noreturn]] inline void validate_failed(const char *file, int line,
                                         const char *condition,
                                         const char *message) {
  log_error("VALIDATE({}) failed at {}:{}: {}", condition, file, line, message);
  std::abort();
}
#define VALIDATE(condition, message)                                           \
  ((condition) ? (void)0                                                       \
               : validate_failed(__FILE__, __LINE__, #condition, message))
#endif
#endif
