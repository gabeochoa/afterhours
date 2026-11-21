#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <list>
#include <unordered_map>
#include <utility>

namespace afterhours {

// Byte-budgeted LRU. Costlier-than-budget puts are rejected, cache unchanged.
// get() pointers die on erase/evict/replace/clear/destroy; for shared handles,
// eviction only drops the cache's reference. Accounted bytes are the sum of
// cost() over held entries -- not process/GPU memory, and bookkeeping is
// charged only if cost() includes it. Single-threaded; callers lock.
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

  // Default cost is sizeof(V): only honest for fixed-size values.
  explicit ByteCache(std::size_t budget_bytes, Cost cost = Cost{})
      : budget_(budget_bytes), cost_(std::move(cost)) {
    if (!cost_) {
      cost_ = [](const V &value) { return sizeof(value); };
    }
  }

  void set_cost(Cost cost) {
    cost_ = std::move(cost);
    if (!cost_) {
      cost_ = [](const V &value) { return sizeof(value); };
    }
    accounted_ = 0;
    for (auto &node : lru_) {
      node.cost = cost_(node.value);
      accounted_ += node.cost;
    }
    evict_until(accounted_);
  }

  void set_budget(std::size_t budget_bytes) {
    budget_ = budget_bytes;
    evict_until(budget_);
  }

  [[nodiscard]] std::size_t budget() const { return budget_; }

  [[nodiscard]] const V *get(const K &key) {
    auto it = index_.find(key);
    if (it == index_.end()) {
      misses_++;
      return nullptr;
    }
    lru_.splice(lru_.begin(), lru_, it->second);
    hits_++;
    return &it->second->value;
  }

  [[nodiscard]] bool contains(const K &key) const {
    return index_.find(key) != index_.end();
  }

  bool put(K key, V value) {
    const std::size_t cost = cost_ ? cost_(value) : sizeof(value);
    if (cost > budget_) {
      rejections_++;
      return false;
    }
    auto existing = index_.find(key);
    if (existing != index_.end()) {
      accounted_ -= existing->second->cost;
      lru_.erase(existing->second);
      index_.erase(existing);
    }
    while (accounted_ + cost > budget_ && !lru_.empty()) {
      evict_lru();
    }
    lru_.push_front(Node{key, std::move(value), cost});
    index_.emplace(lru_.front().key, lru_.begin());
    accounted_ += cost;
    return true;
  }

  // Not counted as an eviction.
  bool erase(const K &key) {
    auto it = index_.find(key);
    if (it == index_.end())
      return false;
    accounted_ -= it->second->cost;
    lru_.erase(it->second);
    index_.erase(it);
    return true;
  }

  // Lifetime counts survive clear(); use reset_stats() to zero them.
  void clear() {
    lru_.clear();
    index_.clear();
    accounted_ = 0;
  }

  // Evicts to `bytes` without changing the configured budget.
  void shrink_to(std::size_t bytes) { evict_until(bytes); }

  [[nodiscard]] std::size_t size() const { return index_.size(); }
  [[nodiscard]] bool empty() const { return index_.empty(); }
  [[nodiscard]] std::size_t accounted_bytes() const { return accounted_; }

  [[nodiscard]] Stats stats() const {
    return Stats{hits_, misses_, evictions_, rejections_, accounted_,
                 index_.size(), budget_};
  }

  void reset_stats() {
    hits_ = 0;
    misses_ = 0;
    evictions_ = 0;
    rejections_ = 0;
  }

private:
  struct Node {
    K key;
    V value;
    std::size_t cost = 0;
  };

  void evict_lru() {
    if (lru_.empty())
      return;
    auto &victim = lru_.back();
    accounted_ -= victim.cost;
    index_.erase(victim.key);
    lru_.pop_back();
    evictions_++;
  }

  void evict_until(std::size_t bytes) {
    while (accounted_ > bytes && !lru_.empty())
      evict_lru();
  }

  std::size_t budget_ = SIZE_MAX;
  Cost cost_;
  std::list<Node> lru_;
  std::unordered_map<K, typename std::list<Node>::iterator, Hash> index_;
  std::size_t accounted_ = 0;
  std::uint64_t hits_ = 0;
  std::uint64_t misses_ = 0;
  std::uint64_t evictions_ = 0;
  std::uint64_t rejections_ = 0;
};

} // namespace afterhours
