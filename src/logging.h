
#pragma once

#include <cstdarg>
#include <iostream>

#if !defined(AFTER_HOURS_REPLACE_LOGGING)
// TODO move to a log.h file and include them in the other parts of the library

inline void log_trace(...) {
  // For now, trace logging is disabled
}

inline void log_info(const char *format, ...) {
  std::cout << "[INFO] ";
  va_list args;
  va_start(args, format);
  vprintf(format, args);
  va_end(args);
  std::cout << std::endl;
}

inline void log_warn(const char *format, ...) {
  std::cout << "[WARN] ";
  va_list args;
  va_start(args, format);
  vprintf(format, args);
  va_end(args);
  std::cout << std::endl;
}

inline void log_error(const char *format, ...) {
  std::cerr << "[ERROR] ";
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  std::cerr << std::endl;
}

inline void log_clean(...) {
  // For now, clean logging is disabled
}

inline void log_once_per(...) {
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
