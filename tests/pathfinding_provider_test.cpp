// pathfinding_provider_test.cpp
// The pathfinding plugin's neighbour/cost callbacks are the pluggable grid
// provider: a consumer can supply its own neighbour topology (e.g. 4-way)
// and step cost (e.g. mud) without forking the plugin. This pins three
// things: the defaults reproduce today's grid paths exactly (parity), a
// custom step cost changes the chosen path, and a custom neighbour set is
// honoured. find_path_* are static, so no ECS init is needed.

// NONE backend: raylib's Vector2 has no operator==, which the unordered_map
// keys in pathfinding need.
#ifndef AFTER_HOURS_USE_NONE
#define AFTER_HOURS_USE_NONE 1
#endif

#define AFTER_HOURS_ENTITY_HELPER
#define AFTER_HOURS_SYSTEM
#include <afterhours/ah.h>
#include <afterhours/src/plugins/pathfinding.h>

#include <cmath>
#include <cstdio>
#include <deque>
#include <functional>
#include <vector>

using namespace afterhours;

static int tests_run = 0, tests_passed = 0;
static void check(bool cond, const char *expr, const char *file, int line) {
  tests_run++;
  if (cond)
    tests_passed++;
  else
    fprintf(stderr, "  FAIL: %s  (%s:%d)\n", expr, file, line);
}
#define CHECK(x) check((x), #x, __FILE__, __LINE__)

using Vec2 = pathfinding::Vec2;

static bool always_walkable(Vec2) { return true; }

static float euclidean(Vec2 a, Vec2 b) {
  float dx = a.x - b.x, dy = a.y - b.y;
  return std::sqrt(dx * dx + dy * dy);
}

static std::vector<Vec2> eight_way(Vec2 pos) {
  std::vector<Vec2> out;
  for (int dx = -1; dx <= 1; dx++)
    for (int dy = -1; dy <= 1; dy++)
      if (dx != 0 || dy != 0)
        out.push_back(Vec2{pos.x + dx, pos.y + dy});
  return out;
}

static std::vector<Vec2> four_way(Vec2 pos) {
  return {Vec2{pos.x + 1, pos.y}, Vec2{pos.x - 1, pos.y},
          Vec2{pos.x, pos.y + 1}, Vec2{pos.x, pos.y - 1}};
}

static bool has_diagonal_step(Vec2 start, const std::deque<Vec2> &path) {
  Vec2 prev = start;
  for (auto p : path) {
    if (std::abs(p.x - prev.x) > 0.5f && std::abs(p.y - prev.y) > 0.5f)
      return true;
    prev = p;
  }
  return false;
}

static bool same_path(const std::deque<Vec2> &a, const std::deque<Vec2> &b) {
  if (a.size() != b.size())
    return false;
  for (size_t i = 0; i < a.size(); i++)
    if (a[i].x != b[i].x || a[i].y != b[i].y)
      return false;
  return true;
}

int main() {
  const Vec2 start{0, 0}, end{4, 3};

  // The shipped defaults: 8-way ±1 neighbours, Euclidean step cost.
  pathfinding::ProvidesPathfinding defaults;
  CHECK(defaults.step_cost_fn(start, Vec2{3, 4}) == 5.0f);

  auto default_path = pathfinding::find_path_astar(
      start, end, always_walkable, defaults.get_neighbors_fn,
      defaults.distance_fn, defaults.step_cost_fn);
  CHECK(!default_path.empty());
  CHECK(default_path.back().x == end.x && default_path.back().y == end.y);
  // Default grid path uses diagonals: 3 diagonal + 1 straight moves.
  CHECK(has_diagonal_step(start, default_path));

  // Parity: a custom provider replicating the defaults gets the same path.
  auto replica_path = pathfinding::find_path_astar(
      start, end, always_walkable, eight_way, euclidean, euclidean);
  CHECK(same_path(default_path, replica_path));

  // A null step cost falls back to the distance heuristic (same path).
  auto fallback_path = pathfinding::find_path_astar(
      start, end, always_walkable, eight_way, euclidean);
  CHECK(same_path(default_path, fallback_path));

  // Custom step cost: diagonals cost 10x, so A* walks orthogonally instead.
  auto steep_diagonals = [](Vec2 a, Vec2 b) {
    float base = euclidean(a, b);
    bool diagonal = std::abs(a.x - b.x) > 0.5f && std::abs(a.y - b.y) > 0.5f;
    return diagonal ? base * 10.0f : base;
  };
  auto costly_path = pathfinding::find_path_astar(
      start, end, always_walkable, eight_way, euclidean, steep_diagonals);
  CHECK(!costly_path.empty());
  CHECK(!has_diagonal_step(start, costly_path));
  CHECK(costly_path.size() > default_path.size());

  // Custom neighbours: 4-way only, so no step may be diagonal.
  auto four_way_path = pathfinding::find_path_astar(
      start, end, always_walkable, four_way, euclidean, euclidean);
  CHECK(!four_way_path.empty());
  CHECK(four_way_path.back().x == end.x && four_way_path.back().y == end.y);
  CHECK(!has_diagonal_step(start, four_way_path));

  // BFS honours a custom neighbour set too (BFS ignores step cost).
  auto bfs_path = pathfinding::find_path_bfs(start, end, always_walkable,
                                             four_way, euclidean);
  CHECK(!bfs_path.empty());
  CHECK(euclidean(bfs_path.front(), end) < 4.0f);

  printf("\n%d/%d checks passed\n", tests_passed, tests_run);
  if (tests_passed != tests_run) {
    printf("FAILURES: %d\n", tests_run - tests_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
