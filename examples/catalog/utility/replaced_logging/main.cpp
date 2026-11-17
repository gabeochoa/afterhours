#define AFTER_HOURS_REPLACE_LOGGING
#include <chrono>
#include <cstdio>
#include <format>
#include <string>

inline int host_lines = 0;

inline void host_sink(const char *level, const std::string &message) {
  host_lines++;
  std::printf("[host %s] %s\n", level, message.c_str());
}

#define HOST_LOG(level, ...)                                                   \
  do {                                                                         \
    host_sink(level, std::format(__VA_ARGS__));                                \
  } while (0)

#define log_trace(...) do {} while (0)
#define log_info(...) HOST_LOG("INFO", __VA_ARGS__)
#define log_warn(...) HOST_LOG("WARN", __VA_ARGS__)
#define log_error(...) HOST_LOG("ERROR", __VA_ARGS__)
#define log_once_per(duration, level, ...)                                     \
  do {                                                                         \
    static std::chrono::steady_clock::time_point last{};                       \
    const auto now = std::chrono::steady_clock::now();                         \
    if (last == std::chrono::steady_clock::time_point{} ||                     \
        now - last >= (duration)) {                                            \
      last = now;                                                              \
      HOST_LOG(level == VENDOR_LOG_ERROR ? "ERROR" : "WARN", __VA_ARGS__);     \
    }                                                                          \
  } while (0)

#include "../../../../ah.h"
#include "../../../../src/crash_handler.h"

int main(int argc, char **) {
  log_trace("dropped {}", 0);
  log_info("info {}", 1);

  if (argc > 1)
    log_error("argc {}", argc);
  else
    log_warn("no args, argc {}", argc);

  for (int i = 0; i < 3; i++)
    log_once_per(std::chrono::minutes(1), VENDOR_LOG_WARN, "once per minute {}",
                 i);

  if (host_lines != 3) {
    std::printf("expected 3 host lines, got %d\n", host_lines);
    return 1;
  }
  std::printf("ok: %d lines through the host sink\n", host_lines);
  return 0;
}
