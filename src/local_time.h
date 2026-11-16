#pragma once

#include <array>
#include <chrono>
#include <ctime>
#include <optional>
#include <string_view>

namespace afterhours {

struct LocalDateTime {
  int year = 0;
  int month = 1;
  int day = 1;
  int weekday = 0;
  int hour = 0;
  int minute = 0;
  int second = 0;

  [[nodiscard]] std::string_view weekday_name() const {
    static constexpr std::array<std::string_view, 7> names = {
        "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    return names[static_cast<size_t>(weekday)];
  }
  [[nodiscard]] std::string_view month_name() const {
    static constexpr std::array<std::string_view, 12> names = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    return names[static_cast<size_t>(month - 1)];
  }
};

[[nodiscard]] inline std::optional<LocalDateTime>
to_local_time(std::chrono::system_clock::time_point when) {
  const std::time_t t = std::chrono::system_clock::to_time_t(when);
  std::tm tm{};
#if defined(_WIN32)
  if (localtime_s(&tm, &t) != 0)
    return std::nullopt;
#else
  if (localtime_r(&t, &tm) == nullptr)
    return std::nullopt;
#endif
  return LocalDateTime{tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                       tm.tm_wday,        tm.tm_hour,    tm.tm_min,
                       tm.tm_sec};
}

[[nodiscard]] inline std::optional<LocalDateTime> local_time_now() {
  return to_local_time(std::chrono::system_clock::now());
}

} // namespace afterhours
