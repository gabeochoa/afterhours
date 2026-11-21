#pragma once

// Window fit/centre/letterbox geometry (header-only, std only). Callers pass
// the display rect; apply sizes with graphics::set_window_size().

#include <algorithm>
#include <cmath>
#include <utility>

namespace afterhours::graphics {

// Clamp to the display, raised to the min (a min past the display wins).
inline std::pair<int, int> fit_window_size(int width, int height,
                                           int display_width,
                                           int display_height,
                                           int min_width = 0,
                                           int min_height = 0) {
  return {std::clamp(width, min_width, std::max(min_width, display_width)),
          std::clamp(height, min_height, std::max(min_height, display_height))};
}

// Centre in a display rect; oversized pins to the display origin.
inline std::pair<int, int> window_centre_position(int display_x,
                                                  int display_y, int display_w,
                                                  int display_h, int win_w,
                                                  int win_h) {
  return {display_x + std::max(0, (display_w - win_w) / 2),
          display_y + std::max(0, (display_h - win_h) / 2)};
}

// Fit the window to the display, then centre it there. Returns {size, pos}.
inline std::pair<std::pair<int, int>, std::pair<int, int>>
centre_window_on_display(int display_x, int display_y, int display_w,
                         int display_h, int win_w, int win_h, int min_w = 0,
                         int min_h = 0) {
  const auto size =
      fit_window_size(win_w, win_h, display_w, display_h, min_w, min_h);
  const auto pos = window_centre_position(display_x, display_y, display_w,
                                          display_h, size.first, size.second);
  return {size, pos};
}

// Largest content-aspect size fitting the window (letterbox/pillarbox).
inline std::pair<int, int> letterbox_content_size(int content_w, int content_h,
                                                  int window_w, int window_h) {
  if (content_w <= 0 || content_h <= 0 || window_w <= 0 || window_h <= 0)
    return {window_w, window_h};
  int dest_w = window_w;
  int dest_h =
      static_cast<int>(std::round((double)dest_w * content_h / content_w));
  if (dest_h > window_h) {
    dest_h = window_h;
    dest_w =
        static_cast<int>(std::round((double)dest_h * content_w / content_h));
  }
  return {dest_w, dest_h};
}

} // namespace afterhours::graphics
