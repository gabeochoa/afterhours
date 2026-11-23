#pragma once


#include <cstddef>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace afterhours::tile_grid {

struct Cell {
  int x = 0;
  int y = 0;
  bool operator==(const Cell &) const = default;
};

template <typename Tile> struct Grid {
  int w = 0;
  int h = 0;
  std::vector<Tile> tiles;

  Grid() = default;
  Grid(int width, int height, Tile fill = Tile{})
      : w(width > 0 ? width : 0), h(height > 0 ? height : 0),
        tiles(static_cast<std::size_t>(w) * static_cast<std::size_t>(h),
              fill) {}

  [[nodiscard]] bool in_bounds(int x, int y) const {
    return x >= 0 && y >= 0 && x < w && y < h;
  }

  [[nodiscard]] bool in_bounds(Cell c) const { return in_bounds(c.x, c.y); }

  [[nodiscard]] std::size_t index(Cell c) const {
    return static_cast<std::size_t>(c.y) * static_cast<std::size_t>(w) +
           static_cast<std::size_t>(c.x);
  }

  [[nodiscard]] Tile at(Cell c) const { return tiles[index(c)]; }

  void set(Cell c, Tile tile) { tiles[index(c)] = tile; }
};

template <typename Tile>
inline int stamp(Grid<Tile> &grid, std::span<const std::string_view> pattern,
                 std::span<const std::pair<char, std::type_identity_t<Tile>>>
                     legend,
                 int origin_x, int origin_y) {
  int written = 0;
  for (std::size_t row = 0; row < pattern.size(); row++) {
    int y = origin_y + static_cast<int>(row);
    if (y < 0 || y >= grid.h)
      continue;
    for (std::size_t col = 0; col < pattern[row].size(); col++) {
      int x = origin_x + static_cast<int>(col);
      if (x < 0 || x >= grid.w)
        continue;
      char ch = pattern[row][col];
      for (const auto &[legend_ch, tile] : legend) {
        if (legend_ch != ch)
          continue;
        grid.set({x, y}, tile);
        written++;
        break;
      }
    }
  }
  return written;
}

}
