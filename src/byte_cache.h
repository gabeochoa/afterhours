#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <list>
#include <unordered_map>
#include <utility>

namespace afterhours {

// A byte-budgeted LRU cache for host-owned values.
//
// floatinghotel carries two implementations of this: `byte_cache.h`
// (token cache, 4 MiB; blob-page cache, 32 MiB) and a second hand-rolled
// accounting cache in `commit_patch_cache.h` (32 MiB). This is the shared
// version both move onto; hanabi's asset/thumbnail caches want the same
// shape. See docs/afterhours_gaps/decisions/decide-memory-budgets.md,
// Option 1.
//
// The host supplies the key type and a cost function `std::size_t(const V&)`
// that says how many bytes a value accounts for (for example a decoded
// image's `rgba.size()`, or a string's `size()` plus its key's size if the
// host wants keys charged too). An entry whose cost is larger than the
// whole budget is rejected: `put` returns false and the cache is left
// unchanged (an existing entry under the same key stays). Otherwise `put`
// evicts least-recently-used entries until the new value fits.
//
// Values are stored by value (moved in) and owned by the cache. `get`
// returns a pointer owned by the cache; it stays valid until that entry
// is erased, evicted, replaced, or the cache is cleared or destroyed.
// If `V` is a shared handle (e.g. `std::shared_ptr`), eviction drops the
// cache's reference, but a copy the caller kept can keep the pointee
// alive after the bytes stop being accounted.
//
// Accounted bytes are NOT process memory. They are exactly the sum of
// `cost(value)` over the entries currently held -- nothing more. The
// bookkeeping itself (list and map nodes, key storage) is not charged
// unless the host's cost function includes it, GPU memory is not
// included, and memory held through shared handles that outlive
// eviction is no longer accounted. Do not report `accounted_bytes()`
// as RSS or total memory.
//
// Single-threaded: no locking inside. Callers that share a cache across
// threads add their own lock, as floatinghotel's blob cache already does.
template <typename K, typename V,
          typename Cost = std::function<std::size_t(const V &)>,
          typename Hash = std::hash<K>>
class ByteCache {
public:
  struct Stats {
    std::uint64_t hits = 0;
    std::uint64_t misses = 0;
    std::uint64_t evictions = 0;
    // Entries refused because their cost exceeded the budget.
    std::uint64_t rejections = 0;
    std::size_t accounted_bytes = 0;
    std::size_t entries = 0;
    std::size_t budget_bytes = 0;
  };

  ByteCache() = default;

  // Default cost accounts `sizeof(V)` per value, which is only honest
  // for fixed-size values; pass a real cost for strings, vectors, images.
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
    // Re-account under the new cost; entries that no longer fit go LRU-first.
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

  // Pointer to the value, or nullptr on a miss. A hit makes the entry
  // the most-recently-used one.
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

  // True when the entry was admitted. False when its cost exceeds the
  // budget: nothing is stored, nothing is evicted, `rejections` grows.
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

  // True when an entry was removed. Not counted as an eviction.
  bool erase(const K &key) {
    auto it = index_.find(key);
    if (it == index_.end())
      return false;
    accounted_ -= it->second->cost;
    lru_.erase(it->second);
    index_.erase(it);
    return true;
  }

  // Removes every entry. Lifetime hit/miss/eviction counts are kept;
  // call reset_stats() to zero them too.
  void clear() {
    lru_.clear();
    index_.clear();
    accounted_ = 0;
  }

  // Evicts LRU entries until at most `bytes` remain accounted. Does not
  // change the configured budget (a memory-pressure handler typically
  // calls `shrink_to(budget() / 2)`).
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
