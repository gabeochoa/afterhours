
#pragma once

#if !defined(AFTER_HOURS_REPLACE_LOGGING)
// TODO eventually implement these
// TODO move to a log.h file and include them in the other parts of the library
inline void log_trace(...) {}
inline void log_info(...) {}
inline void log_warn(...) {}
inline void log_error(...) {}
inline void log_clean(...) {}
#endif

#if !defined(AFTER_HOURS_REPLACE_VALIDATE)
inline void VALIDATE(...) {}
#endif
