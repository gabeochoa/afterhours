#pragma once

#include <atomic>
#include <chrono>
#include <deque>
#include <functional>
#include <limits>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../core/base_component.h"
#include "../core/entity_helper.h"
#include "../core/system.h"
#include "../developer.h"
#include "../logging.h"

// Vector2Type is defined in developer.h

// Hash for grid coords. Safe as an unordered key because A*/BFS never iterate
// these tables in sorted order.
template <> struct std::hash<Vector2Type> {
  size_t operator()(const Vector2Type &v) const noexcept {
    size_t hx = std::hash<float>{}(v.x);
    size_t hy = std::hash<float>{}(v.y);
    return hx ^ (hy + 0x9e3779b97f4a7c15ULL + (hx << 6) + (hx >> 2));
  }
};

namespace afterhours {

struct pathfinding : developer::Plugin {
  using Vec2 = Vector2Type;

  enum class Algorithm { AStar, BFS };

  // Pathfinding request/response structures
  struct PathRequest {
    int entity_id;
    Vec2 start;
    Vec2 end;
    std::function<void(const std::deque<Vec2> &)> on_complete;
  };

  struct PathResponse {
    int entity_id;
    std::deque<Vec2> path;
    std::function<void(const std::deque<Vec2> &)> on_complete;
  };

  // Thread-safe queue implementation
  template <typename T> struct AtomicQueue {
    void push_back(const T &value) {
      std::lock_guard<std::mutex> lock(m_mutex);
      q.push_back(value);
    }

    void pop_front() {
      std::lock_guard<std::mutex> lock(m_mutex);
      q.pop_front();
    }

    [[nodiscard]] T &front() {
      std::lock_guard<std::mutex> lock(m_mutex);
      return q.front();
    }

    [[nodiscard]] bool empty() const {
      std::lock_guard<std::mutex> lock(m_mutex);
      return q.empty();
    }

    [[nodiscard]] size_t size() const {
      std::lock_guard<std::mutex> lock(m_mutex);
      return q.size();
    }

  private:
    std::deque<T> q;
    mutable std::mutex m_mutex;
  };

  // Component for entities that can pathfind
  struct CanPathfind : BaseComponent {
    std::deque<Vec2> path;
    Vec2 start;
    Vec2 goal;
    bool has_active_request = false;
    int path_size = 0;
    size_t max_path_length = 0;

    [[nodiscard]] bool is_path_empty() const { return path.empty(); }

    [[nodiscard]] std::deque<Vec2> get_path() const { return path; }

    void update_path(const std::deque<Vec2> &new_path) {
      path = new_path;
      path_size = static_cast<int>(path.size());
      has_active_request = false;
      max_path_length = std::max(max_path_length, path.size());
    }
  };

  // Component that provides walkability data (optional)
  struct HasWalkabilityMap : BaseComponent {
    std::function<bool(Vec2)> is_walkable;
    HasWalkabilityMap() = default;
    explicit HasWalkabilityMap(const std::function<bool(Vec2)> &checker)
        : is_walkable(checker) {}
  };

  // Singleton component for pathfinding manager
  struct ProvidesPathfinding : BaseComponent {
    Algorithm current_algorithm = Algorithm::BFS;
    std::function<bool(Vec2)> walkability_checker;
    std::function<float(Vec2, Vec2)> distance_fn;
    std::function<std::vector<Vec2>(Vec2)> get_neighbors_fn;
    AtomicQueue<PathRequest> request_queue;
    AtomicQueue<PathResponse> response_queue;
    std::atomic<bool> running{false};
    std::vector<Vec2> entities_storage;
    std::mutex entities_mutex;

    ProvidesPathfinding() {
      // Default distance function (Euclidean)
      distance_fn = [](Vec2 a, Vec2 b) {
        float dx = a.x - b.x;
        float dy = a.y - b.y;
        return std::sqrt(dx * dx + dy * dy);
      };

      // Default neighbor function (8-directional)
      get_neighbors_fn = [](Vec2 pos) {
        std::vector<Vec2> neighbors;
        neighbors.reserve(8);
        static constexpr int neigh_x[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
        static constexpr int neigh_y[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
        for (int i = 0; i < 8; i++) {
          neighbors.push_back(Vec2{pos.x + static_cast<float>(neigh_x[i]),
                                   pos.y + static_cast<float>(neigh_y[i])});
        }
        return neighbors;
      };
    }
  };

  // BFS pathfinding implementation
  static std::deque<Vec2>
  find_path_bfs(Vec2 start, Vec2 end,
                const std::function<bool(Vec2)> &is_walkable,
                const std::function<std::vector<Vec2>(Vec2)> &get_neighbors,
                const std::function<float(Vec2, Vec2)> &distance,
                int max_path_length = 50) {
    struct Node {
      Vec2 pos;
      Node *parent = nullptr;
    };

    std::unordered_set<Vec2> visited;
    // deque never relocates elements, so &arena.back() stays valid as a parent.
    std::deque<Node> arena;
    std::deque<Node *> queue;

    auto reconstruct_path = [](Node *n) {
      std::deque<Vec2> path;
      Node *cur = n;
      while (cur) {
        path.push_back(cur->pos);
        cur = cur->parent;
      }
      return path;
    };

    visited.insert(start);
    arena.push_back(Node{.pos = start, .parent = nullptr});
    queue.push_back(&arena.back());

    while (!queue.empty()) {
      Node *n = queue.front();
      queue.pop_front();

      float dist_sq = distance(n->pos, end);
      if (dist_sq < 4.0f) {
        return reconstruct_path(n);
      }

      if (dist_sq > (max_path_length * max_path_length)) {
        continue;
      }

      auto neighbors = get_neighbors(n->pos);
      for (const auto &neighbor : neighbors) {
        if (!is_walkable(neighbor)) {
          continue;
        }
        if (visited.contains(neighbor)) {
          continue;
        }
        visited.insert(neighbor);
        arena.push_back(Node{.pos = neighbor, .parent = n});
        queue.push_back(&arena.back());
      }
    }

    return {};
  }

  // A* pathfinding implementation
  static std::deque<Vec2>
  find_path_astar(Vec2 start, Vec2 end,
                  const std::function<bool(Vec2)> &is_walkable,
                  const std::function<std::vector<Vec2>(Vec2)> &get_neighbors,
                  const std::function<float(Vec2, Vec2)> &distance) {
    struct ScoreValue {
      float value;
      ScoreValue() { value = std::numeric_limits<float>::max(); }
      explicit ScoreValue(float v) : value(v) {}
      explicit operator float() const { return value; }
    };

    if (!is_walkable(end)) {
      return {};
    }

    struct OpenEntry {
      float f;
      Vec2 pos;
    };
    struct OpenCmp {
      bool operator()(const OpenEntry &a, const OpenEntry &b) const {
        // Min-heap by f, breaking f-ties toward the smallest Vec2.
        if (a.f != b.f)
          return a.f > b.f;
        return b.pos < a.pos;
      }
    };
    std::priority_queue<OpenEntry, std::vector<OpenEntry>, OpenCmp> open_set;
    open_set.push(OpenEntry{static_cast<float>(distance(start, end)), start});

    std::unordered_map<Vec2, Vec2> parent_map;
    std::unordered_map<Vec2, ScoreValue> g_score;
    std::unordered_map<Vec2, ScoreValue> f_score;

    g_score[start] = ScoreValue(0.0f);
    f_score[start] = ScoreValue(distance(start, end));

    auto reconstruct_path = [](std::unordered_map<Vec2, Vec2> &parent_map,
                               Vec2 current) {
      std::deque<Vec2> path;
      while (parent_map.contains(current)) {
        path.push_front(current);
        current = parent_map[current];
      }
      return path;
    };

    int iterations = 0;
    constexpr int MAX_ITERATIONS = 10000;

    while (!open_set.empty()) {
      OpenEntry top = open_set.top();
      open_set.pop();

      // Lazy deletion: a node can have several queued entries; skip any whose f
      // no longer matches its current f_score.
      if (top.f != static_cast<float>(f_score[top.pos])) {
        continue;
      }

      iterations++;
      if (iterations > MAX_ITERATIONS) {
        log_warn("A* pathfinding hit iteration limit");
        break;
      }

      Vec2 current = top.pos;

      float current_dist = distance(current, end);
      if (current_dist < 0.5f) {
        auto path = reconstruct_path(parent_map, current);
        path.push_back(end);
        return path;
      }

      auto neighbors = get_neighbors(current);
      for (const auto &neighbor : neighbors) {
        float tentative_g =
            static_cast<float>(g_score[current]) + distance(current, neighbor);
        float neighbor_g = static_cast<float>(g_score[neighbor]);

        if (tentative_g < neighbor_g) {
          parent_map[neighbor] = current;
          g_score[neighbor] = ScoreValue(tentative_g);
          float nf = tentative_g + distance(neighbor, end);
          f_score[neighbor] = ScoreValue(nf);
          open_set.push(OpenEntry{nf, neighbor});
        }
      }
    }

    return {};
  }

  // System to process pathfinding requests
  struct PathfindingRequestSystem : System<ProvidesPathfinding> {
    virtual void for_each_with(Entity &, ProvidesPathfinding &,
                               float) override {
      // Process requests in the background thread
      // This system just ensures the thread is running
    }
  };

  // System to process pathfinding responses
  struct PathfindingResponseSystem : System<ProvidesPathfinding> {
    virtual void for_each_with(Entity &, ProvidesPathfinding &provider,
                               float) override {
      while (!provider.response_queue.empty()) {
        const auto &response = provider.response_queue.front();

        auto entity = EntityHelper::getEntityForID(response.entity_id);
        if (entity) {
          if (entity.asE().has<CanPathfind>()) {
            entity.asE().get<CanPathfind>().update_path(response.path);
            if (response.on_complete) {
              response.on_complete(response.path);
            }
          }
        } else {
          log_warn("Path requester %d no longer exists", response.entity_id);
        }
        provider.response_queue.pop_front();
      }
    }
  };

  static void add_singleton_components(Entity &entity) {
    entity.addComponent<ProvidesPathfinding>();
    EntityHelper::registerSingleton<ProvidesPathfinding>(entity);
  }

  static void enforce_singletons(SystemManager &sm) {
    sm.register_update_system(
        std::make_unique<developer::EnforceSingleton<ProvidesPathfinding>>());
  }

  static void register_update_systems(SystemManager &sm) {
    sm.register_update_system(std::make_unique<PathfindingRequestSystem>());
    sm.register_update_system(std::make_unique<PathfindingResponseSystem>());
  }

  // API methods
  static ProvidesPathfinding *get_provider() {
    return EntityHelper::get_singleton_cmp<ProvidesPathfinding>();
  }

  static void init() {
    auto *provider = get_provider();
    if (provider) {
      log_warn("Pathfinding plugin already initialized");
      return;
    }

    Entity &entity = EntityHelper::createPermanentEntity();
    add_singleton_components(entity);
    EntityHelper::merge_entity_arrays();
  }

  static std::thread start() {
    auto *provider = get_provider();
    if (!provider) {
      log_error("Pathfinding plugin not initialized. Call pathfinding::init() "
                "first.");
      return std::thread{};
    }

    if (provider->running.load()) {
      log_warn("Pathfinding thread already running");
      return std::thread{};
    }

    provider->running = true;
    return std::thread([provider]() {
      constexpr float desired_frame_rate = 240.0f;
      constexpr std::chrono::duration<float> fixed_time_step(
          1.0f / desired_frame_rate);

      auto previous_time = std::chrono::high_resolution_clock::now();
      auto current_time = previous_time;

      while (provider->running.load()) {
        current_time = std::chrono::high_resolution_clock::now();
        float duration =
            std::chrono::duration_cast<std::chrono::duration<float>>(
                current_time - previous_time)
                .count();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      current_time - previous_time)
                      .count();

        if (ms < 3) {
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        if (duration <= fixed_time_step.count()) {
          continue;
        }
        previous_time = current_time;

        while (!provider->request_queue.empty()) {
          const auto &request = provider->request_queue.front();
          std::deque<Vec2> path;

          std::function<bool(Vec2)> is_walkable = provider->walkability_checker;
          if (!is_walkable) {
            // Try to get from HasWalkabilityMap singleton
            auto *walkability_provider =
                EntityHelper::get_singleton_cmp<HasWalkabilityMap>();
            if (walkability_provider) {
              is_walkable = walkability_provider->is_walkable;
            }
          }

          if (is_walkable) {
            if (provider->current_algorithm == Algorithm::AStar) {
              path = find_path_astar(request.start, request.end, is_walkable,
                                     provider->get_neighbors_fn,
                                     provider->distance_fn);
            } else {
              path = find_path_bfs(request.start, request.end, is_walkable,
                                   provider->get_neighbors_fn,
                                   provider->distance_fn);
            }
          }

          provider->request_queue.pop_front();
          provider->response_queue.push_back(
              PathResponse{.entity_id = request.entity_id,
                           .path = path,
                           .on_complete = request.on_complete});
        }
      }
    });
  }

  static void stop() {
    auto *provider = get_provider();
    if (!provider) {
      return;
    }
    provider->running = false;
  }

  static void request_path(
      int entity_id, Vec2 start, Vec2 end,
      const std::function<void(const std::deque<Vec2> &)> &callback = nullptr) {
    auto *provider = get_provider();
    if (!provider) {
      log_error("Pathfinding plugin not initialized. Call pathfinding::init() "
                "first.");
      return;
    }

    provider->request_queue.push_back(PathRequest{.entity_id = entity_id,
                                                  .start = start,
                                                  .end = end,
                                                  .on_complete = callback});
  }

  static std::deque<Vec2>
  find_path_sync(Vec2 start, Vec2 end,
                 const std::function<bool(Vec2)> &is_walkable) {
    auto *provider = get_provider();
    if (!provider) {
      log_error("Pathfinding plugin not initialized. Call pathfinding::init() "
                "first.");
      return {};
    }

    if (provider->current_algorithm == Algorithm::AStar) {
      return find_path_astar(start, end, is_walkable,
                             provider->get_neighbors_fn, provider->distance_fn);
    } else {
      return find_path_bfs(start, end, is_walkable, provider->get_neighbors_fn,
                           provider->distance_fn);
    }
  }

  static void set_algorithm(Algorithm algo) {
    auto *provider = get_provider();
    if (!provider) {
      log_error("Pathfinding plugin not initialized. Call pathfinding::init() "
                "first.");
      return;
    }
    provider->current_algorithm = algo;
  }

  static void
  register_walkability_checker(const std::function<bool(Vec2)> &checker) {
    auto *provider = get_provider();
    if (!provider) {
      log_error("Pathfinding plugin not initialized. Call pathfinding::init() "
                "first.");
      return;
    }
    provider->walkability_checker = checker;
  }
};

// Compile-time verification that pathfinding satisfies the PluginCore concept
static_assert(developer::PluginCore<pathfinding>,
              "pathfinding must implement the core plugin interface");

} // namespace afterhours
