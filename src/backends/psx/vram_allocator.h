#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>


namespace afterhours::psx {

struct VramRect {
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;

  bool valid() const { return w > 0 && h > 0; }
  std::size_t units() const {
    return valid() ? static_cast<std::size_t>(w) * static_cast<std::size_t>(h)
                   : 0;
  }
  std::size_t bytes() const { return units() * 2u; }
};

class VramAllocator {
public:
  VramAllocator() = default;
  VramAllocator(int width_px, int height_px) { reset(width_px, height_px); }

  void reset(int width_px, int height_px) {
    width_ = width_px > 0 ? width_px : 0;
    height_ = height_px > 0 ? height_px : 0;
    used_.assign(static_cast<std::size_t>(width_) *
                     static_cast<std::size_t>(height_),
                 0);
    used_count_ = 0;
  }

  void clear() { reset(width_, height_); }

  int width() const { return width_; }
  int height() const { return height_; }

  std::optional<VramRect> allocate(int w, int h) {
    if (w <= 0 || h <= 0 || w > width_ || h > height_)
      return std::nullopt;
    for (int y = 0; y + h <= height_; ++y) {
      for (int x = 0; x + w <= width_;) {
        const int blocked = first_blocked_column(x, y, w, h);
        if (blocked < 0) {
          const VramRect rect{x, y, w, h};
          mark(rect, 1);
          return rect;
        }
        x = blocked + 1;
      }
    }
    return std::nullopt;
  }

  bool reserve(VramRect rect) {
    if (!rect.valid() || rect.x < 0 || rect.y < 0 ||
        rect.x + rect.w > width_ || rect.y + rect.h > height_)
      return false;
    if (first_blocked_column(rect.x, rect.y, rect.w, rect.h) >= 0)
      return false;
    mark(rect, 1);
    return true;
  }

  void free(VramRect rect) {
    if (!rect.valid())
      return;
    const int x0 = rect.x < 0 ? 0 : rect.x;
    const int y0 = rect.y < 0 ? 0 : rect.y;
    const int x1 = rect.x + rect.w > width_ ? width_ : rect.x + rect.w;
    const int y1 = rect.y + rect.h > height_ ? height_ : rect.y + rect.h;
    mark(VramRect{x0, y0, x1 - x0, y1 - y0}, 0);
  }

  std::size_t total_units() const { return used_.size(); }
  std::size_t used_units() const { return used_count_; }
  std::size_t free_units() const { return total_units() - used_count_; }
  std::size_t total_bytes() const { return total_units() * 2u; }
  std::size_t used_bytes() const { return used_units() * 2u; }
  std::size_t free_bytes() const { return free_units() * 2u; }

  float fraction_used() const {
    if (total_units() == 0)
      return 0.f;
    return static_cast<float>(used_count_) /
           static_cast<float>(total_units());
  }

  bool occupied(int x, int y) const {
    if (x < 0 || y < 0 || x >= width_ || y >= height_)
      return false;
    return used_[index(x, y)] != 0;
  }

  std::string map_ascii(int cols = 64, int rows = 32) const {
    if (width_ == 0 || height_ == 0 || cols <= 0 || rows <= 0)
      return {};
    std::string out;
    out.reserve(static_cast<std::size_t>(rows) *
                (static_cast<std::size_t>(cols) + 1));
    for (int r = 0; r < rows; ++r) {
      const int y0 = r * height_ / rows;
      const int y1 = (r + 1) * height_ / rows;
      for (int c = 0; c < cols; ++c) {
        const int x0 = c * width_ / cols;
        const int x1 = (c + 1) * width_ / cols;
        bool any = false;
        for (int y = y0; y < y1 && !any; ++y)
          for (int x = x0; x < x1 && !any; ++x)
            any = used_[index(x, y)] != 0;
        out.push_back(any ? '#' : '.');
      }
      if (r + 1 < rows)
        out.push_back('\n');
    }
    return out;
  }

private:
  std::size_t index(int x, int y) const {
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) +
           static_cast<std::size_t>(x);
  }

  int first_blocked_column(int x, int y, int w, int h) const {
    for (int yy = y; yy < y + h; ++yy)
      for (int xx = x; xx < x + w; ++xx)
        if (used_[index(xx, yy)] != 0)
          return xx;
    return -1;
  }

  void mark(VramRect rect, std::uint8_t value) {
    for (int y = rect.y; y < rect.y + rect.h; ++y) {
      for (int x = rect.x; x < rect.x + rect.w; ++x) {
        std::uint8_t &cell = used_[index(x, y)];
        if (value != 0 && cell == 0)
          ++used_count_;
        else if (value == 0 && cell != 0)
          --used_count_;
        cell = value;
      }
    }
  }

  int width_ = 0;
  int height_ = 0;
  std::vector<std::uint8_t> used_;
  std::size_t used_count_ = 0;
};

}
