#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <list>
#include <unordered_map>
#include <utility>

namespace afterhours {

template <typename K, typename V,
          typename Cost = std::function<std::size_t(const V &)>,
          typename Hash = std::hash<K>>
class ByteCache {
public:
  struct Stats {
    std::uint64_t hits = 0;
    std::uint64_t misses = 0;
    std::uint64_t evictions = 0;
    std::uint64_t rejections = 0;
    std::size_t accounted_bytes = 0;
    std::size_t entries = 0;
    std::size_t budget_bytes = 0;
  };

  ByteCache() = default;

  explicit ByteCache(std::size_t bytes, Cost cost = Cost{})
      : budget_bytes(bytes), cost_fn(std::move(cost)) {
    if (!cost_fn) {
      cost_fn = [](const V &value) { return sizeof(value); };
    }
  }

  void set_cost(Cost cost) {
    cost_fn = std::move(cost);
    if (!cost_fn) {
      cost_fn = [](const V &value) { return sizeof(value); };
    }
    accounted = 0;
    for (auto &node : lru_list) {
      node.cost = cost_fn(node.value);
      accounted += node.cost;
    }
    evict_until(accounted);
  }

  void set_budget(std::size_t bytes) {
    budget_bytes = bytes;
    evict_until(budget_bytes);
  }

  [[nodiscard]] std::size_t budget() const { return budget_bytes; }

  [[nodiscard]] const V *get(const K &key) {
    auto it = index.find(key);
    if (it == index.end()) {
      miss_count++;
      return nullptr;
    }
    lru_list.splice(lru_list.begin(), lru_list, it->second);
    hit_count++;
    return &it->second->value;
  }

  [[nodiscard]] bool contains(const K &key) const {
    return index.find(key) != index.end();
  }

  bool put(K key, V value) {
    const std::size_t cost = cost_fn ? cost_fn(value) : sizeof(value);
    if (cost > budget_bytes) {
      reject_count++;
      return false;
    }
    auto existing = index.find(key);
    if (existing != index.end()) {
      accounted -= existing->second->cost;
      lru_list.erase(existing->second);
      index.erase(existing);
    }
    while (accounted + cost > budget_bytes && !lru_list.empty()) {
      evict_lru();
    }
    lru_list.push_front(Node{key, std::move(value), cost});
    index.emplace(lru_list.front().key, lru_list.begin());
    accounted += cost;
    return true;
  }

  bool erase(const K &key) {
    auto it = index.find(key);
    if (it == index.end())
      return false;
    accounted -= it->second->cost;
    lru_list.erase(it->second);
    index.erase(it);
    return true;
  }

  void clear() {
    lru_list.clear();
    index.clear();
    accounted = 0;
  }

  void shrink_to(std::size_t bytes) { evict_until(bytes); }

  [[nodiscard]] std::size_t size() const { return index.size(); }
  [[nodiscard]] bool empty() const { return index.empty(); }
  [[nodiscard]] std::size_t accounted_bytes() const { return accounted; }

  [[nodiscard]] Stats stats() const {
    return Stats{hit_count, miss_count, evict_count, reject_count, accounted,
                 index.size(), budget_bytes};
  }

  void reset_stats() {
    hit_count = 0;
    miss_count = 0;
    evict_count = 0;
    reject_count = 0;
  }

private:
  struct Node {
    K key;
    V value;
    std::size_t cost = 0;
  };

  void evict_lru() {
    if (lru_list.empty())
      return;
    auto &victim = lru_list.back();
    accounted -= victim.cost;
    index.erase(victim.key);
    lru_list.pop_back();
    evict_count++;
  }

  void evict_until(std::size_t bytes) {
    while (accounted > bytes && !lru_list.empty())
      evict_lru();
  }

  std::size_t budget_bytes = SIZE_MAX;
  Cost cost_fn;
  std::list<Node> lru_list;
  std::unordered_map<K, typename std::list<Node>::iterator, Hash> index;
  std::size_t accounted = 0;
  std::uint64_t hit_count = 0;
  std::uint64_t miss_count = 0;
  std::uint64_t evict_count = 0;
  std::uint64_t reject_count = 0;
};

}
