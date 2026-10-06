#pragma once

// LRU cache bounded by host-reported byte costs. See docs/plugins.md
// "Byte-budget cache" for the contract. Not thread-safe: wrap it in a mutex
// when shared.

#include <cstddef>
#include <functional>
#include <list>
#include <unordered_map>
#include <utility>

namespace afterhours {

struct CacheStats {
  std::size_t hits = 0;
  std::size_t misses = 0;
  std::size_t evictions = 0;  // entries dropped to make room
  std::size_t rejections = 0; // puts whose cost exceeded the budget
  std::size_t bytes = 0;      // sum of costs of resident entries
  std::size_t entries = 0;
  std::size_t budget = 0;
};

// Values are moved in and owned by the cache. Use Value = shared_ptr<const T>
// to hand out shared references; an evicted value then lives on while
// referenced, and those bytes are no longer counted here. Accounting is
// exactly the sum of cost(key, value): list nodes, hash buckets and the
// duplicated key are excluded unless the cost function adds them.
template <class Key, class Value, class Hash = std::hash<Key>,
          class KeyEqual = std::equal_to<Key>>
class ByteBudgetCache {
public:
  using CostFn = std::function<std::size_t(const Key &, const Value &)>;

  ByteBudgetCache(std::size_t budget, CostFn cost)
      : budget_(budget), cost_(std::move(cost)) {}

  // Pointer valid until the next put/erase/clear. Counts a hit or miss and
  // marks a hit most recently used.
  const Value *get(const Key &key) {
    auto it = index_.find(key);
    if (it == index_.end()) {
      ++stats_.misses;
      return nullptr;
    }
    ++stats_.hits;
    entries_.splice(entries_.begin(), entries_, it->second);
    return &it->second->value;
  }

  bool contains(const Key &key) const { return index_.count(key) != 0; }

  // Inserts or replaces, then evicts least recently used entries until the
  // budget holds. A value costing more than the whole budget is rejected and
  // returns false; any older value under that key is removed either way, so
  // a stale value never outlives a failed replacement.
  bool put(Key key, Value value) {
    const std::size_t cost = cost_(key, value);
    erase(key);
    if (cost > budget_) {
      ++stats_.rejections;
      return false;
    }
    while (stats_.bytes > budget_ - cost) {
      drop(std::prev(entries_.end()));
      ++stats_.evictions;
    }
    entries_.push_front({key, std::move(value), cost});
    index_.emplace(std::move(key), entries_.begin());
    stats_.bytes += cost;
    return true;
  }

  bool erase(const Key &key) {
    auto it = index_.find(key);
    if (it == index_.end()) return false;
    drop(it->second);
    return true;
  }

  void clear() {
    entries_.clear();
    index_.clear();
    stats_.bytes = 0;
  }

  // Lowering the budget evicts immediately.
  void set_budget(std::size_t budget) {
    budget_ = budget;
    while (stats_.bytes > budget_) {
      drop(std::prev(entries_.end()));
      ++stats_.evictions;
    }
  }

  CacheStats stats() const {
    CacheStats out = stats_;
    out.entries = entries_.size();
    out.budget = budget_;
    return out;
  }

private:
  struct Entry {
    Key key;
    Value value;
    std::size_t cost;
  };
  using Iter = typename std::list<Entry>::iterator;

  std::size_t budget_;
  CostFn cost_;
  std::list<Entry> entries_;
  std::unordered_map<Key, Iter, Hash, KeyEqual> index_;
  CacheStats stats_;

  void drop(Iter it) {
    stats_.bytes -= it->cost;
    index_.erase(it->key);
    entries_.erase(it);
  }
};

} // namespace afterhours
