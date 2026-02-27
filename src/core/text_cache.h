#pragma once

#include <concepts>
#include <cstdint>
#include <functional>
#include <list>
#include <string>
#include <string_view>
#include <unordered_map>

#include "../developer.h"
#include "base_component.h"

namespace afterhours {

namespace ui {

using MeasureTextFn =
    std::function<Vector2Type(std::string_view text, std::string_view font_name,
                              float font_size, float spacing)>;

template <typename F>
concept TextMeasureFn =
    requires(F fn, std::string_view text, std::string_view font_name,
             float font_size, float spacing) {
      { fn(text, font_name, font_size, spacing) } -> std::same_as<Vector2Type>;
    };

class TextMeasureCache : public BaseComponent {
public:
  static constexpr uint32_t DEFAULT_PRUNE_INTERVAL = 60;
  static constexpr uint32_t DEFAULT_MAX_AGE = 120;
  static constexpr size_t DEFAULT_MAX_ENTRIES = 4096;

private:
  struct LRUEntry {
    uint64_t key;
    Vector2Type size;
    uint32_t last_used_generation = 0;
  };

  using LRUList = std::list<LRUEntry>;
  using LRUIter = LRUList::iterator;

  MeasureTextFn measure_fn_;
  LRUList lru_list_;
  std::unordered_map<uint64_t, LRUIter> lookup_;

  uint32_t current_generation_ = 0;
  uint32_t prune_interval_ = DEFAULT_PRUNE_INTERVAL;
  uint32_t max_age_ = DEFAULT_MAX_AGE;
  size_t max_entries_ = DEFAULT_MAX_ENTRIES;

  uint64_t cache_hits_ = 0;
  uint64_t cache_misses_ = 0;

public:
  TextMeasureCache() = default;

  template <TextMeasureFn F>
  explicit TextMeasureCache(F measure_fn)
      : measure_fn_(std::move(measure_fn)) {}

  template <TextMeasureFn F> void set_measure_function(F fn) {
    measure_fn_ = std::move(fn);
  }

  void set_prune_interval(uint32_t frames) { prune_interval_ = frames; }
  void set_max_age(uint32_t frames) { max_age_ = frames; }
  void set_max_entries(size_t count) { max_entries_ = count; }

  [[nodiscard]] Vector2Type measure(std::string_view text,
                                    std::string_view font_name, float font_size,
                                    float spacing = 1.0f) {
    if (!measure_fn_) {
      return {0.0f, 0.0f};
    }

    uint64_t key = compute_hash(text, font_name, font_size, spacing);
    auto it = lookup_.find(key);
    if (it != lookup_.end()) {
      it->second->last_used_generation = current_generation_;
      lru_list_.splice(lru_list_.begin(), lru_list_, it->second);
      cache_hits_++;
      return it->second->size;
    }

    cache_misses_++;
    Vector2Type size = measure_fn_(text, font_name, font_size, spacing);

    while (lookup_.size() >= max_entries_) {
      evict_lru();
    }

    lru_list_.push_front({key, size, current_generation_});
    lookup_[key] = lru_list_.begin();
    return size;
  }

  [[nodiscard]] float measure_width(std::string_view text,
                                    std::string_view font_name, float font_size,
                                    float spacing = 1.0f) {
    return measure(text, font_name, font_size, spacing).x;
  }

  [[nodiscard]] float measure_height(std::string_view text,
                                     std::string_view font_name,
                                     float font_size, float spacing = 1.0f) {
    return measure(text, font_name, font_size, spacing).y;
  }

  void end_frame() {
    current_generation_++;
    if (prune_interval_ > 0 && current_generation_ % prune_interval_ == 0) {
      prune_stale_entries();
    }
  }

  void prune() { prune_stale_entries(); }

  void clear() {
    lru_list_.clear();
    lookup_.clear();
    cache_hits_ = 0;
    cache_misses_ = 0;
  }

  [[nodiscard]] size_t size() const { return lookup_.size(); }
  [[nodiscard]] uint64_t hits() const { return cache_hits_; }
  [[nodiscard]] uint64_t misses() const { return cache_misses_; }
  [[nodiscard]] uint32_t generation() const { return current_generation_; }

  [[nodiscard]] float hit_rate() const {
    uint64_t total = cache_hits_ + cache_misses_;
    return total > 0 ? static_cast<float>(cache_hits_) / total * 100.0f : 0.0f;
  }

  void reset_stats() {
    cache_hits_ = 0;
    cache_misses_ = 0;
  }

  [[nodiscard]] static uint64_t compute_hash(std::string_view text,
                                             std::string_view font_name,
                                             float font_size, float spacing) {
    constexpr uint64_t FNV_OFFSET = 14695981039346656037ULL;
    constexpr uint64_t FNV_PRIME = 1099511628211ULL;

    uint64_t hash = FNV_OFFSET;
    for (char c : text) {
      hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
      hash *= FNV_PRIME;
    }
    hash ^= 0xFF;
    hash *= FNV_PRIME;

    for (char c : font_name) {
      hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
      hash *= FNV_PRIME;
    }

    const auto *size_bytes = reinterpret_cast<const uint8_t *>(&font_size);
    for (size_t i = 0; i < sizeof(float); ++i) {
      hash ^= size_bytes[i];
      hash *= FNV_PRIME;
    }

    const auto *spacing_bytes = reinterpret_cast<const uint8_t *>(&spacing);
    for (size_t i = 0; i < sizeof(float); ++i) {
      hash ^= spacing_bytes[i];
      hash *= FNV_PRIME;
    }

    return hash;
  }

private:
  void evict_lru() {
    if (lru_list_.empty())
      return;
    auto &victim = lru_list_.back();
    lookup_.erase(victim.key);
    lru_list_.pop_back();
  }

  void prune_stale_entries() {
    if (current_generation_ < max_age_)
      return;

    uint32_t threshold = current_generation_ - max_age_;
    auto it = lru_list_.end();
    while (it != lru_list_.begin()) {
      --it;
      if (it->last_used_generation < threshold) {
        lookup_.erase(it->key);
        it = lru_list_.erase(it);
      }
    }
  }
};

} // namespace ui

} // namespace afterhours
