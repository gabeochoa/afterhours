#pragma once


#include <cstddef>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace afterhours::tile_grid {

struct Cell {
  int x = 0;
  int y = 0;
  bool operator==(const Cell &) const = default;
};

struct Grid {
  int w = 0;
  int h = 0;
  std::vector<int> tiles;

  Grid() = default;
  Grid(int width, int height, int fill = 0)
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
  [[nodiscard]] int at(Cell c) const { return tiles[index(c)]; }
  void set(Cell c, int tile) { tiles[index(c)] = tile; }
};

inline int stamp(Grid &grid, std::span<const std::string_view> pattern,
                 std::span<const std::pair<char, int>> legend, int origin_x,
                 int origin_y) {
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

template <typename Passable>
[[nodiscard]] std::vector<char> reachable(const Grid &grid, Cell start,
                                          Passable passable) {
  std::vector<char> mask(grid.tiles.size(), 0);
  if (!grid.in_bounds(start) || !passable(grid.at(start)))
    return mask;
  std::vector<Cell> queue{start};
  mask[grid.index(start)] = 1;
  for (std::size_t head = 0; head < queue.size(); head++) {
    Cell c = queue[head];
    constexpr Cell kDirs[4] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
    for (Cell d : kDirs) {
      Cell n{c.x + d.x, c.y + d.y};
      if (!grid.in_bounds(n) || mask[grid.index(n)] ||
          !passable(grid.at(n)))
        continue;
      mask[grid.index(n)] = 1;
      queue.push_back(n);
    }
  }
  return mask;
}

template <typename Passable>
[[nodiscard]] std::vector<Cell>
unreachable_targets(const Grid &grid, Cell start,
                    std::span<const Cell> targets, Passable passable) {
  std::vector<char> mask = reachable(grid, start, passable);
  std::vector<Cell> out;
  for (Cell t : targets)
    if (!grid.in_bounds(t) || !mask[grid.index(t)])
      out.push_back(t);
  return out;
}

template <typename Passable>
[[nodiscard]] bool all_targets_reachable(const Grid &grid, Cell start,
                                         std::span<const Cell> targets,
                                         Passable passable) {
  return unreachable_targets(grid, start, targets, passable).empty();
}

}
