#include <afterhours/src/tile_grid.h>

#include <cstdio>
#include <string_view>
#include <utility>
#include <vector>

using namespace afterhours::tile_grid;

static int checks_run = 0;
static int checks_passed = 0;
static void check(bool cond, const char *expr, int line) {
  checks_run++;
  if (cond)
    checks_passed++;
  else
    fprintf(stderr, "  FAIL: %s  (line %d)\n", expr, line);
}
#define CHECK(expr) check((expr), #expr, __LINE__)

static bool open_tile(int tile) { return tile != 1; }

int main() {
  {
    Grid g(5, 3, 0);
    const std::pair<char, int> legend[] = {{'#', 1}, {'.', 2}, {'@', 3}};
    const std::string_view room[] = {"###", "#.#", "###"};
    CHECK(stamp(g, room, legend, 0, 0) == 9);
    CHECK(g.at({1, 1}) == 2);
    const std::string_view actor[] = {" @ "};
    CHECK(stamp(g, actor, legend, 0, 1) == 1);
    CHECK(g.at({1, 1}) == 3);
    CHECK(g.at({0, 1}) == 1);
  }

  {
    Grid g(3, 3, 0);
    const std::pair<char, int> legend[] = {{'#', 1}};
    const std::string_view block[] = {"##", "##"};
    CHECK(stamp(g, block, legend, -1, -1) == 1);
    CHECK(g.at({0, 0}) == 1);
    CHECK(stamp(g, block, legend, 2, 2) == 1);
    CHECK(g.at({2, 2}) == 1);
    CHECK(stamp(g, block, legend, 10, 10) == 0);
  }

  {
    const std::pair<char, int> legend[] = {{'#', 1}};
    const std::string_view walls[] = {"#####", "#...#", "#####", "#...#",
                                      "#####"};
    Grid walled(5, 5, 0);
    stamp(walled, walls, legend, 0, 0);
    auto mask = reachable(walled, {1, 1}, open_tile);
    CHECK(mask[walled.index({2, 1})] == 1);
    CHECK(mask[walled.index({1, 3})] == 0);
    const Cell targets[] = {{2, 1}, {1, 3}, {9, 9}};
    CHECK(!all_targets_reachable(walled, {1, 1}, targets, open_tile));
    auto missing = unreachable_targets(walled, {1, 1}, targets, open_tile);
    CHECK(missing.size() == 2);
    CHECK(missing[0] == (Cell{1, 3}));
    CHECK(missing[1] == (Cell{9, 9}));
    const Cell ok[] = {{2, 1}, {3, 1}};
    CHECK(all_targets_reachable(walled, {1, 1}, ok, open_tile));
    CHECK(reachable(walled, {0, 0}, open_tile) ==
          std::vector<char>(25, 0));
    CHECK(reachable(walled, {-1, 0}, open_tile) ==
          std::vector<char>(25, 0));
  }

  {
    const std::pair<char, int> legend[] = {{'#', 1}, {'.', 2}};
    const std::string_view pat[] = {"#..#", ".##.", "#..#"};
    Grid a(6, 4, 0), b(6, 4, 0);
    for (int run = 0; run < 50; run++) {
      Grid c(6, 4, 0);
      stamp(c, pat, legend, 1, 0);
      if (run == 0)
        a = c;
      b = c;
      CHECK(a.tiles == b.tiles);
      CHECK(reachable(a, {1, 1}, open_tile) ==
            reachable(b, {1, 1}, open_tile));
    }
  }

  printf("%d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
