#pragma once

// Turn a crash into a report.
//
// The library is where the aborts come from -- a missing singleton, an empty
// gen_first_enforce, every VALIDATE -- so the handler belongs here too.
//
// Reports to stderr and log_sink_fn, so log_file.h picks it up. Not through
// log_error: a host that replaced logging with one that aborts would re-enter
// the handler from inside the report.

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>

#include "logging.h"

// execinfo.h/backtrace() only exist where the platform ships them. Web
// (Emscripten) builds are clang and non-Windows, so they hit the old include
// and have neither header nor functions; wordproc needed a web-only compat
// shim that upstreamed this. Probe the header instead of the compiler.
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__) && __has_include(<execinfo.h>)
#define AFTER_HOURS_HAS_EXECINFO 1
#include <execinfo.h>
#endif

#if defined(__EMSCRIPTEN__) && __has_include(<emscripten.h>)
// Emscripten has no execinfo; it reports its own callstack instead.
#include <emscripten.h>
#define AFTER_HOURS_HAS_EMSCRIPTEN_STACK 1
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

inline void report_line(const std::string &message) {
  std::fprintf(stderr, "[ERROR] %s\n", message.c_str());
  if (log_sink_fn)
    log_sink_fn("[ERROR]", message.c_str());
}

inline volatile std::sig_atomic_t &reporting() {
  static volatile std::sig_atomic_t active = 0;
  return active;
}

inline void write_backtrace() {
#ifdef AFTER_HOURS_HAS_EXECINFO
  void *frames[64];
  const int count = backtrace(frames, 64);
  char **symbols = backtrace_symbols(frames, count);
  if (symbols == nullptr)
    return;
  // Skip the handler's own frames.
  for (int i = 2; i < count; i++) {
    char index[16];
    std::snprintf(index, sizeof(index), "  #%-2d ", i - 2);
    report_line(std::string(index) + symbols[i]);
  }
  std::free(symbols);
#elif defined(AFTER_HOURS_HAS_EMSCRIPTEN_STACK)
  if (const char *stack = emscripten_get_callstack())
    report_line(stack);
#else
  report_line("stack trace unavailable on this platform");
#endif
}

inline void report(const char *what) {
  if (reporting())
    return;
  reporting() = 1;
  report_line(std::string("---- afterhours crash: ") + what + " ----");
  if (running_system() != nullptr)
    report_line(std::string("while running system: ") + running_system());
  else
    report_line("not inside a system");
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
#ifdef SIGBUS
  case SIGBUS:
    name = "SIGBUS (misaligned or unmapped access)";
    break;
#endif
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
  for (int sig : {SIGSEGV, SIGABRT,
#ifdef SIGBUS
                  SIGBUS,
#endif
                  SIGILL, SIGFPE})
    std::signal(sig, detail::on_signal);
  std::set_terminate(detail::on_terminate);
}

// What the report names as the running system. SystemManager sets this.
inline void set_running_system(const char *name) {
  detail::running_system() = name;
}

} // namespace crash
} // namespace afterhours
