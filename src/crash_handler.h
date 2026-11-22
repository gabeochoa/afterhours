#pragma once

// Turn a crash into a report.
//
// The library is where the aborts come from -- a missing singleton, an empty
// gen_first_enforce, every VALIDATE -- so the handler belongs here too.
//
// Reports to stderr and log_sink_fn, so log_file.h picks it up. Not through
// log_error: a host that replaced logging with one that aborts would re-enter
// the handler from inside the report.

#include <cstddef>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>

#include "logging.h"
#include "warn_once.h"

#if (defined(__GNUC__) || defined(__clang__)) && !defined(_WIN32)
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
#if (defined(__GNUC__) || defined(__clang__)) && !defined(_WIN32)
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
#endif
}

inline void write_backtrace_to(std::FILE *out) {
#if (defined(__GNUC__) || defined(__clang__)) && !defined(_WIN32)
  void *frames[64];
  const int count = backtrace(frames, 64);
  char **symbols = backtrace_symbols(frames, count);
  if (symbols == nullptr)
    return;
  for (int i = 2; i < count; i++)
    std::fprintf(out, "  #%-2d %s\n", i - 2, symbols[i]);
  std::free(symbols);
#else
  warn_once(0, "@notimplemented crash::write_backtrace_to on this platform");
  (void)out;
#endif
}

inline constexpr std::size_t tail_line_count = 48;
inline constexpr std::size_t tail_line_length = 256;

inline char (&tail_lines())[tail_line_count][tail_line_length] {
  static char lines[tail_line_count][tail_line_length] = {};
  return lines;
}

inline volatile std::sig_atomic_t &tail_written() {
  static volatile std::sig_atomic_t count = 0;
  return count;
}

inline void tail_append(const char *level, const char *message) {
  char *slot = tail_lines()[tail_written() % tail_line_count];
  std::size_t n = 0;
  for (const char *p = level; *p && n + 1 < tail_line_length; p++)
    slot[n++] = *p;
  if (n + 1 < tail_line_length)
    slot[n++] = ' ';
  for (const char *p = message; *p && n + 1 < tail_line_length; p++)
    slot[n++] = (*p == '\n' || *p == '\r') ? ' ' : *p;
  slot[n] = '\0';
  tail_written() = tail_written() + 1;
}

inline void (*&previous_sink())(const char *, const char *) {
  static void (*sink)(const char *, const char *) = nullptr;
  return sink;
}

inline void tail_sink(const char *level, const char *message) {
  tail_append(level, message);
  if (previous_sink())
    previous_sink()(level, message);
}

inline bool &sink_chained() {
  static bool chained = false;
  return chained;
}

inline constexpr std::size_t report_path_length = 1024;

inline char *report_path_buffer() {
  static char path[report_path_length] = {};
  return path;
}

inline bool &installed() {
  static bool on = false;
  return on;
}

inline std::terminate_handler &previous_terminate() {
  static std::terminate_handler handler = nullptr;
  return handler;
}

inline constexpr int fatal_signals[] = {SIGSEGV, SIGABRT,
#ifdef SIGBUS
                                        SIGBUS,
#endif
                                        SIGILL, SIGFPE};

inline bool write_report_file(const char *what) {
  if (report_path_buffer()[0] == '\0')
    return false;
  std::FILE *out = std::fopen(report_path_buffer(), "w");
  if (out == nullptr)
    return false;
  std::fprintf(out, "---- afterhours crash: %s ----\n", what);
  if (running_system() != nullptr)
    std::fprintf(out, "while running system: %s\n", running_system());
  else
    std::fprintf(out, "not inside a system\n");
  std::fprintf(out, "backtrace:\n");
  write_backtrace_to(out);
  const std::size_t written = static_cast<std::size_t>(tail_written());
  const std::size_t count =
      written < tail_line_count ? written : tail_line_count;
  std::fprintf(out, "recent log (last %zu lines):\n", count);
  for (std::size_t i = written - count; i < written; i++)
    std::fprintf(out, "%s\n", tail_lines()[i % tail_line_count]);
  std::fclose(out);
  return true;
}

inline void report(const char *what) {
  if (reporting())
    return;
  reporting() = 1;
  write_report_file(what);
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
  if (detail::installed())
    return;
  detail::installed() = true;
  for (int sig : detail::fatal_signals)
    std::signal(sig, detail::on_signal);
  detail::previous_terminate() = std::set_terminate(detail::on_terminate);
}

inline void install(const char *report_path) {
  if (report_path != nullptr && report_path[0] != '\0') {
    std::strncpy(detail::report_path_buffer(), report_path,
                 detail::report_path_length - 1);
    detail::report_path_buffer()[detail::report_path_length - 1] = '\0';
  }
  if (!detail::sink_chained()) {
    detail::previous_sink() = log_sink_fn;
    log_sink_fn = detail::tail_sink;
    detail::sink_chained() = true;
  }
  install_handler();
}

inline void uninstall() {
  for (int sig : detail::fatal_signals)
    std::signal(sig, SIG_DFL);
  if (detail::previous_terminate() != nullptr)
    std::set_terminate(detail::previous_terminate());
  if (detail::sink_chained() && log_sink_fn == detail::tail_sink)
    log_sink_fn = detail::previous_sink();
  detail::previous_sink() = nullptr;
  detail::sink_chained() = false;
  detail::report_path_buffer()[0] = '\0';
  detail::installed() = false;
}

// What the report names as the running system. SystemManager sets this.
inline void set_running_system(const char *name) {
  detail::running_system() = name;
}

} // namespace crash
} // namespace afterhours
