#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "tile_grid.h"

namespace afterhours::tile_grid {

template <typename Tile, typename Passable>
[[nodiscard]] std::vector<char> reachable(const Grid<Tile> &grid, Cell start,
                                          Passable passable) {
  std::vector<char> mask(grid.tiles.size(), 0);
  if (!grid.in_bounds(start) || !passable(grid.at(start)))
    return mask;
  std::vector<Cell> queue{start};
  mask[grid.index(start)] = 1;
  for (std::size_t head = 0; head < queue.size(); head++) {
    Cell c = queue[head];
    constexpr Cell DIRS[4] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
    for (Cell d : DIRS) {
      Cell n{c.x + d.x, c.y + d.y};
      if (!grid.in_bounds(n) || mask[grid.index(n)] || !passable(grid.at(n)))
        continue;
      mask[grid.index(n)] = 1;
      queue.push_back(n);
    }
  }
  return mask;
}

template <typename Tile, typename Passable>
[[nodiscard]] std::vector<Cell>
unreachable_targets(const Grid<Tile> &grid, Cell start,
                    std::span<const Cell> targets, Passable passable) {
  std::vector<char> mask = reachable(grid, start, passable);
  std::vector<Cell> out;
  for (Cell t : targets)
    if (!grid.in_bounds(t) || !mask[grid.index(t)])
      out.push_back(t);
  return out;
}

template <typename Tile, typename Passable>
[[nodiscard]] bool all_targets_reachable(const Grid<Tile> &grid, Cell start,
                                         std::span<const Cell> targets,
                                         Passable passable) {
  return unreachable_targets(grid, start, targets, passable).empty();
}

}
