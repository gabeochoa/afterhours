#pragma once

#include <concepts>
#include <cstdint>
#include <functional>
#include <list>
#include <string>
#include <string_view>
#include <unordered_map>

#include "../byte_cache.h"
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
  static constexpr size_t kEntryCost = sizeof(uint64_t) + sizeof(Vector2Type);

  MeasureTextFn measure_fn_;
  ByteCache<uint64_t, Vector2Type> cache_{
      DEFAULT_MAX_ENTRIES * kEntryCost,
      [](const Vector2Type &) { return kEntryCost; }};
  std::unordered_map<uint64_t, uint32_t> last_used_generation_;

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
  void set_max_entries(size_t count) {
    max_entries_ = count;
    cache_.set_budget((count == 0 ? 1 : count) * kEntryCost);
    if (last_used_generation_.size() > cache_.size()) {
      std::unordered_map<uint64_t, uint32_t> kept;
      for (const auto &[key, gen] : last_used_generation_)
        if (cache_.contains(key))
          kept.emplace(key, gen);
      last_used_generation_ = std::move(kept);
    }
  }

  [[nodiscard]] Vector2Type measure(std::string_view text,
                                    std::string_view font_name, float font_size,
                                    float spacing = 1.0f) {
    if (!measure_fn_) {
      return {0.0f, 0.0f};
    }

    uint64_t key = compute_hash(text, font_name, font_size, spacing);
    if (const Vector2Type *cached = cache_.get(key)) {
      last_used_generation_[key] = current_generation_;
      cache_hits_++;
      return *cached;
    }

    cache_misses_++;
    Vector2Type size = measure_fn_(text, font_name, font_size, spacing);
    if (cache_.put(key, size))
      last_used_generation_[key] = current_generation_;
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
    cache_.clear();
    last_used_generation_.clear();
    cache_hits_ = 0;
    cache_misses_ = 0;
  }

  [[nodiscard]] size_t size() const { return cache_.size(); }
  [[nodiscard]] size_t accounted_bytes() const {
    return cache_.accounted_bytes();
  }
  [[nodiscard]] ByteCache<uint64_t, Vector2Type>::Stats byte_stats() const {
    return cache_.stats();
  }
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
  void prune_stale_entries() {
    if (current_generation_ < max_age_)
      return;

    uint32_t threshold = current_generation_ - max_age_;
    for (auto it = last_used_generation_.begin();
         it != last_used_generation_.end();) {
      if (it->second < threshold) {
        cache_.erase(it->first);
        it = last_used_generation_.erase(it);
      } else {
        ++it;
      }
    }
  }
};

} // namespace ui

} // namespace afterhours
