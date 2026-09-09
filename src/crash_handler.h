#pragma once

// Turn a crash into a report.
//
// The library is where the aborts come from -- a missing singleton, an empty
// gen_first_enforce, every VALIDATE -- so the handler belongs here too.
//
// Reports through log_error, so log_file.h picks it up. Without that it only
// helps someone already at a terminal.

#include <csignal>
#include <cstdlib>
#include <exception>

#include "logging.h"

#if defined(__GNUC__) || defined(__clang__)
#include <cxxabi.h>
#include <execinfo.h>
#endif

namespace afterhours {
namespace crash {

namespace detail {

// Which system is running. SystemManager sets it. A stack trace usually
// can't say, since the crash is several frames down in library code.
inline const char *&running_system() {
  static const char *name = nullptr;
  return name;
}

inline void write_backtrace() {
#if defined(__GNUC__) || defined(__clang__)
  void *frames[64];
  const int count = backtrace(frames, 64);
  char **symbols = backtrace_symbols(frames, count);
  if (symbols == nullptr)
    return;
  // Skip the handler's own frames.
  for (int i = 2; i < count; i++)
    log_error("  #{:<2} {}", i - 2, symbols[i]);
  std::free(symbols);
#endif
}

inline void report(const char *what) {
  log_error("---- afterhours crash: {} ----", what);
  if (running_system() != nullptr)
    log_error("while running system: {}", running_system());
  else
    log_error("not inside a system");
  // No flush needed, the sink writes a line at a time.
  write_backtrace();
}

inline void on_signal(int sig) {
  const char *name = "signal";
  switch (sig) {
  case SIGSEGV:
    name = "SIGSEGV (bad memory access)";
    break;
  case SIGABRT:
    name = "SIGABRT (abort, usually a failed invariant)";
    break;
  case SIGBUS:
    name = "SIGBUS (misaligned or unmapped access)";
    break;
  case SIGILL:
    name = "SIGILL (illegal instruction)";
    break;
  case SIGFPE:
    name = "SIGFPE (arithmetic error)";
    break;
  default:
    break;
  }
  report(name);
  // Re-raise so the exit status and any debugger still see a crash.
  std::signal(sig, SIG_DFL);
  std::raise(sig);
}

inline void on_terminate() {
  report("std::terminate (uncaught exception)");
  std::abort();
}

} // namespace detail

// Call once at startup. Fine before the log file exists; it buffers.
inline void install_handler() {
  for (int sig : {SIGSEGV, SIGABRT, SIGBUS, SIGILL, SIGFPE})
    std::signal(sig, detail::on_signal);
  std::set_terminate(detail::on_terminate);
}

// What the report names as the running system. SystemManager sets this.
inline void set_running_system(const char *name) {
  detail::running_system() = name;
}

} // namespace crash
} // namespace afterhours
