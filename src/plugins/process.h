#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "../logging.h"

#if defined(_WIN32)
#include <process.h>
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <unistd.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

namespace afterhours {
namespace process {

inline std::optional<std::string> current_executable_path() {
#if defined(_WIN32)
  char buffer[MAX_PATH];
  const DWORD length = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
  if (length == 0 || length == MAX_PATH)
    return std::nullopt;
  return std::string(buffer, length);
#elif defined(__APPLE__)
  char buffer[1024];
  std::uint32_t size = sizeof(buffer);
  if (_NSGetExecutablePath(buffer, &size) != 0)
    return std::nullopt;
  return std::string(buffer);
#elif defined(__linux__)
  char buffer[1024];
  const ssize_t length = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
  if (length <= 0)
    return std::nullopt;
  buffer[length] = '\0';
  return std::string(buffer);
#else
  log_error("@notimplemented process::current_executable_path on this platform");
  return std::nullopt;
#endif
}

inline std::vector<const char *>
build_argv(std::string_view exe, std::span<const std::string> args) {
  std::vector<const char *> argv;
  argv.reserve(args.size() + 2);
  argv.push_back(exe.data());
  for (const auto &arg : args)
    argv.push_back(arg.c_str());
  argv.push_back(nullptr);
  return argv;
}

inline bool relaunch_self(std::span<const std::string> args = {}) {
  const auto exe = current_executable_path();
  if (!exe)
    return false;
  auto argv = build_argv(*exe, args);
#if defined(_WIN32)
  const intptr_t result = _spawnv(_P_OVERLAY, exe->c_str(), argv.data());
  (void)result;
  _exit(127);
#elif defined(__APPLE__) || defined(__linux__)
  execv(exe->c_str(), const_cast<char *const *>(argv.data()));
  return false;
#else
  log_error("@notimplemented process::relaunch_self on this platform");
  return false;
#endif
}

} // namespace process
} // namespace afterhours
