#pragma once

#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <vector>

#include "../config.h"
#if !defined(AFTER_HOURS_USE_NONE)
#include "../graphics.h"
#endif
#include "../local_time.h"
#include "files.h"

namespace afterhours::bug_note {

struct Result {
  std::string note_path;
  std::string screenshot_path;
  bool screenshot_captured = false;
};

inline Result capture(const std::filesystem::path &dir,
                      std::string_view text, std::string_view build_id = {},
                      const std::vector<std::string> &context = {}) {
  Result result{};
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);

  const auto now = local_time_now();
  const std::string stem =
      now ? std::format("bug-{:04d}{:02d}{:02d}-{:02d}{:02d}{:02d}",
                        now->year, now->month, now->day, now->hour,
                        now->minute, now->second)
          : std::format(
                "bug-{}",
                std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count());

  std::filesystem::path note_path = dir / std::format("{}.md", stem);
  for (int n = 1; std::filesystem::exists(note_path); ++n)
    note_path = dir / std::format("{}-{}.md", stem, n);

  std::filesystem::path png_path = note_path;
  png_path.replace_extension(".png");
  result.note_path = note_path.string();
  result.screenshot_path = png_path.string();

#if !defined(AFTER_HOURS_USE_NONE)
  result.screenshot_captured = graphics::capture_frame(png_path);
#endif

  std::string context_block;
  for (const auto &line : context)
    context_block += std::format("- {}\n", line);
  if (context_block.empty())
    context_block = "(none)\n";

  const std::string screenshot =
      result.screenshot_captured
          ? result.screenshot_path
#if defined(AFTER_HOURS_USE_NONE)
          : "unavailable (none backend has no frame to capture)";
#else
          : "unavailable (no frame captured)";
#endif

  files::write_string_atomic(
      note_path,
      std::format("# Playtest bug note\n\n"
                  "timestamp: {}\n"
                  "build: {}\n\n"
                  "## Note\n\n{}\n\n"
                  "## Context\n\n{}"
                  "\nscreenshot: {}\n",
                  now ? std::format("{:04d}-{:02d}-{:02d} "
                                    "{:02d}:{:02d}:{:02d} local",
                                    now->year, now->month, now->day, now->hour,
                                    now->minute, now->second)
                      : "unknown",
                  build_id.empty() ? "unknown" : build_id, text,
                  context_block, screenshot));

  return result;
}

} // namespace afterhours::bug_note
