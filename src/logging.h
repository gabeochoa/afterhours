
#pragma once

#include <format>
#include <iostream>
#include <string_view>

#if !defined(AFTER_HOURS_REPLACE_LOGGING)

// C++20 format-based logging with {} placeholders

template <typename... Args>
inline void log_trace(std::format_string<Args...>, Args &&...) {
  // For now, trace logging is disabled
}

template <typename... Args>
inline void log_info(std::format_string<Args...> fmt, Args &&...args) {
  std::cout << "[INFO] " << std::format(fmt, std::forward<Args>(args)...)
            << std::endl;
}

template <typename... Args>
inline void log_warn(std::format_string<Args...> fmt, Args &&...args) {
  std::cout << "[WARN] " << std::format(fmt, std::forward<Args>(args)...)
            << std::endl;
}

template <typename... Args>
inline void log_error(std::format_string<Args...> fmt, Args &&...args) {
  std::cerr << "[ERROR] " << std::format(fmt, std::forward<Args>(args)...)
            << std::endl;
}

template <typename... Args>
inline void log_clean(std::format_string<Args...>, Args &&...) {
  // For now, clean logging is disabled
}

template <typename Duration, typename... Args>
inline void log_once_per(Duration, int, std::format_string<Args...>, Args &&...) {
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
