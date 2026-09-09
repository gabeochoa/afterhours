#pragma once

// Writes the log to a file as well as the console. stdout is nothing to a
// double-clicked .app.
//
// Include it only where you want it: logging.h is on the ECS core path, so it
// keeps a function pointer and this keeps the buffer and its includes.
//
// The path isn't known until files::init runs, so lines are buffered until
// then.

#include <cstdio>
#include <string>
#include <vector>

#include "logging.h"

namespace afterhours {
namespace log_file {

// A run that never opens a file must not grow without bound.
inline constexpr std::size_t max_buffered_lines = 512;

namespace detail {

inline std::FILE *&handle() {
  static std::FILE *f = nullptr;
  return f;
}

inline std::vector<std::string> &pending() {
  static std::vector<std::string> lines;
  return lines;
}

// Per line, not per buffer: a crash takes an unflushed tail with it.
inline void write_line(const char *level, const char *message) {
  if (handle() == nullptr) {
    if (pending().size() < max_buffered_lines)
      pending().push_back(std::string(level) + " " + message);
    return;
  }
  std::fprintf(handle(), "%s %s\n", level, message);
  std::fflush(handle());
}

// On include, not on open(): otherwise the startup lines are gone before a
// path is known, and those are the ones you want.
inline const bool buffering_from_include = (log_sink_fn = write_line, true);

} // namespace detail

// Truncates, then writes out whatever was buffered.
inline void open(const char *path) {
  if (detail::handle() != nullptr)
    std::fclose(detail::handle());
  detail::handle() = std::fopen(path, "w");
  if (detail::handle() == nullptr) {
    std::fprintf(stderr, "[ERROR] could not open log file %s\n", path);
    return;
  }
  for (const auto &line : detail::pending())
    std::fprintf(detail::handle(), "%s\n", line.c_str());
  std::fflush(detail::handle());
  detail::pending().clear();
  detail::pending().shrink_to_fit();

}

inline void close() {
  if (detail::handle() == nullptr)
    return;
  std::fflush(detail::handle());
  std::fclose(detail::handle());
  detail::handle() = nullptr;
}

} // namespace log_file
} // namespace afterhours
