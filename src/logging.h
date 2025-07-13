
#pragma once

#if !defined(AFTER_HOURS_REPLACE_LOGGING)
// TODO eventually implement these
// TODO move to a log.h file and include them in the other parts of the library
inline void log_trace(...) {}
inline void log_info(...) {}
inline void log_warn(...) {}
inline void log_error(...) {}
inline void log_clean(...) {}
inline void log_once_per(...) {}
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
