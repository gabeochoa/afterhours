#pragma once


#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "config.h"
#if !defined(AFTER_HOURS_USE_NONE)
#include "graphics.h"
#endif
#include "local_time.h"

namespace afterhours::bug_note {

struct Result {
  std::string note_path;
  std::string screenshot_path;
  bool screenshot_captured = false;
};

namespace detail {

inline std::string pad(int v, int width = 2) {
  std::string s = std::to_string(v);
  while (static_cast<int>(s.size()) < width)
    s = "0" + s;
  return s;
}

inline std::string stamp_filename(const SerializableLocalDateTime &t) {
  return "bug-" + pad(t.year, 4) + pad(t.month) + pad(t.day) + "-" +
         pad(t.hour) + pad(t.minute) + pad(t.second);
}

inline std::string stamp_human(const SerializableLocalDateTime &t) {
  return pad(t.year, 4) + "-" + pad(t.month) + "-" + pad(t.day) + " " +
         pad(t.hour) + ":" + pad(t.minute) + ":" + pad(t.second) + " local";
}

}

inline Result capture(const std::filesystem::path &dir,
                      std::string_view text, std::string_view build_id = {},
                      const std::vector<std::string> &context = {}) {
  Result result{};
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);

  const auto now = local_time_now();
  std::string stem = now
      ? detail::stamp_filename(*now)
      : "bug-" + std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
              std::chrono::system_clock::now().time_since_epoch())
                  .count());

  std::filesystem::path note_path = dir / (stem + ".md");
  for (int n = 1; std::filesystem::exists(note_path); ++n)
    note_path = dir / (stem + "-" + std::to_string(n) + ".md");

  std::filesystem::path png_path = note_path;
  png_path.replace_extension(".png");
  result.note_path = note_path.string();
  result.screenshot_path = png_path.string();

#if !defined(AFTER_HOURS_USE_NONE)
  result.screenshot_captured = graphics::capture_frame(png_path);
#endif

  std::ofstream out(note_path);
  out << "# Playtest bug note\n\n";
  out << "timestamp: "
      << (now ? detail::stamp_human(*now) : std::string("unknown")) << "\n";
  out << "build: " << (build_id.empty() ? "unknown" : std::string(build_id))
      << "\n\n";
  out << "## Note\n\n" << text << "\n\n";
  out << "## Context\n\n";
  if (context.empty())
    out << "(none)\n";
  for (const auto &line : context)
    out << "- " << line << "\n";
  out << "\nscreenshot: ";
  if (result.screenshot_captured)
    out << result.screenshot_path;
  else {
    out << "unavailable";
#if defined(AFTER_HOURS_USE_NONE)
    out << " (none backend has no frame to capture)";
#else
    out << " (no frame captured)";
#endif
  }
  out << "\n";

  return result;
}

}
