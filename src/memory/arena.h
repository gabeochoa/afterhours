#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <type_traits>
#include <utility>

namespace afterhours {

namespace detail {

inline bool is_power_of_two(size_t value) {
  return value != 0 && (value & (value - 1)) == 0;
}

inline size_t align_up_value(size_t value, size_t alignment) {
  return (value + alignment - 1) & ~(alignment - 1);
}

inline void *aligned_alloc_compat(size_t alignment, size_t size) {
  // Prefer std::aligned_alloc when available; fall back to posix_memalign.
#if defined(__APPLE__)
  void *ptr = nullptr;
  if (posix_memalign(&ptr, alignment, size) != 0)
    return nullptr;
  return ptr;
#else
  return std::aligned_alloc(alignment, size);
#endif
}

} // namespace detail

class Arena {
public:
  static constexpr size_t DEFAULT_CAPACITY = 4 * 1024 * 1024; // 4MB
  static constexpr size_t DEFAULT_ALIGNMENT = 16;

private:
  uint8_t *memory_ = nullptr;
  size_t capacity_ = 0;
  size_t offset_ = 0;
  size_t alignment_ = DEFAULT_ALIGNMENT;
  bool owns_memory_ = false;

  size_t peak_usage_ = 0;
  size_t allocation_count_ = 0;

public:
  Arena() = default;

  explicit Arena(size_t capacity, size_t alignment = DEFAULT_ALIGNMENT)
      : alignment_(alignment), owns_memory_(true) {
    init_owned(capacity, alignment);
  }

  Arena(void *memory, size_t capacity, size_t alignment = DEFAULT_ALIGNMENT)
      : memory_(static_cast<uint8_t *>(memory)), capacity_(capacity),
        alignment_(alignment), owns_memory_(false) {}

  ~Arena() {
    if (owns_memory_ && memory_) {
      std::free(memory_);
    }
  }

  Arena(const Arena &) = delete;
  Arena &operator=(const Arena &) = delete;

  Arena(Arena &&other) noexcept { swap(other); }
  Arena &operator=(Arena &&other) noexcept {
    if (this != &other) {
      if (owns_memory_ && memory_) {
        std::free(memory_);
      }
      memory_ = nullptr;
      swap(other);
    }
    return *this;
  }

  [[nodiscard]] void *allocate(size_t size) {
    size_t aligned_offset = detail::align_up_value(offset_, alignment_);
    if (aligned_offset + size > capacity_) {
      return nullptr;
    }

    void *ptr = memory_ + aligned_offset;
    offset_ = aligned_offset + size;
    allocation_count_++;
    if (offset_ > peak_usage_) {
      peak_usage_ = offset_;
    }
    return ptr;
  }

  template <typename T, typename... Args>
  [[nodiscard]] T *create(Args &&...args) {
    void *ptr = allocate(sizeof(T));
    if (!ptr)
      return nullptr;
    return new (ptr) T(std::forward<Args>(args)...);
  }

  template <typename T> [[nodiscard]] T *create_array(size_t count) {
    if (count == 0)
      return nullptr;

    void *ptr = allocate(sizeof(T) * count);
    if (!ptr)
      return nullptr;

    T *array = static_cast<T *>(ptr);
    for (size_t i = 0; i < count; ++i) {
      new (&array[i]) T();
    }
    return array;
  }

  template <typename T>
  [[nodiscard]] T *create_array_uninitialized(size_t count) {
    if (count == 0)
      return nullptr;
    return static_cast<T *>(allocate(sizeof(T) * count));
  }

  void reset() {
    offset_ = 0;
    allocation_count_ = 0;
  }

  void reset_stats() {
    reset();
    peak_usage_ = 0;
  }

  [[nodiscard]] size_t used() const { return offset_; }
  [[nodiscard]] size_t capacity() const { return capacity_; }
  [[nodiscard]] size_t remaining() const { return capacity_ - offset_; }
  [[nodiscard]] size_t peak_usage() const { return peak_usage_; }
  [[nodiscard]] size_t allocation_count() const { return allocation_count_; }

  [[nodiscard]] float usage_percent() const {
    return capacity_ > 0
               ? static_cast<float>(offset_) / capacity_ * 100.0f
               : 0.0f;
  }

  [[nodiscard]] float peak_usage_percent() const {
    return capacity_ > 0
               ? static_cast<float>(peak_usage_) / capacity_ * 100.0f
               : 0.0f;
  }

  [[nodiscard]] bool is_valid() const { return memory_ != nullptr; }

private:
  void init_owned(size_t capacity, size_t alignment) {
    assert(detail::is_power_of_two(alignment) &&
           "Arena alignment must be a power of two");
    alignment_ = std::max(alignment, alignof(std::max_align_t));
    size_t aligned_capacity = detail::align_up_value(capacity, alignment_);
    memory_ = static_cast<uint8_t *>(
        detail::aligned_alloc_compat(alignment_, aligned_capacity));
    assert(memory_ && "Arena allocation failed");
    capacity_ = aligned_capacity;
  }

  void swap(Arena &other) noexcept {
    std::swap(memory_, other.memory_);
    std::swap(capacity_, other.capacity_);
    std::swap(offset_, other.offset_);
    std::swap(alignment_, other.alignment_);
    std::swap(owns_memory_, other.owns_memory_);
    std::swap(peak_usage_, other.peak_usage_);
    std::swap(allocation_count_, other.allocation_count_);
  }
};

template <typename T> class ArenaVector {
  static_assert(std::is_trivially_destructible_v<T>,
                "ArenaVector requires trivially destructible T");
  static_assert(std::is_move_constructible_v<T>,
                "ArenaVector requires move-constructible T");

private:
  T *data_ = nullptr;
  size_t size_ = 0;
  size_t capacity_ = 0;
  Arena *arena_ = nullptr;

public:
  ArenaVector() = default;

  explicit ArenaVector(Arena &arena, size_t initial_capacity = 64)
      : capacity_(initial_capacity), arena_(&arena) {
    if (initial_capacity > 0) {
      data_ = arena.create_array_uninitialized<T>(initial_capacity);
    }
  }

  void push_back(const T &value) {
    if (size_ >= capacity_) {
      grow();
    }
    new (&data_[size_]) T(value);
    size_++;
  }

  void push_back(T &&value) {
    if (size_ >= capacity_) {
      grow();
    }
    new (&data_[size_]) T(std::move(value));
    size_++;
  }

  template <typename... Args> T &emplace_back(Args &&...args) {
    if (size_ >= capacity_) {
      grow();
    }
    new (&data_[size_]) T(std::forward<Args>(args)...);
    return data_[size_++];
  }

  T &operator[](size_t index) { return data_[index]; }
  const T &operator[](size_t index) const { return data_[index]; }

  [[nodiscard]] size_t size() const { return size_; }
  [[nodiscard]] bool empty() const { return size_ == 0; }
  [[nodiscard]] T *data() { return data_; }
  [[nodiscard]] const T *data() const { return data_; }

  T *begin() { return data_; }
  T *end() { return data_ + size_; }
  const T *begin() const { return data_; }
  const T *end() const { return data_ + size_; }

  void clear() { size_ = 0; }

private:
  void grow() {
    assert(arena_ && "ArenaVector requires a valid arena");
    size_t new_capacity = capacity_ == 0 ? 8 : capacity_ * 2;
    T *new_data = arena_->create_array_uninitialized<T>(new_capacity);

    if (data_ && size_ > 0) {
      for (size_t i = 0; i < size_; ++i) {
        new (&new_data[i]) T(std::move(data_[i]));
      }
    }

    data_ = new_data;
    capacity_ = new_capacity;
  }
};

template <typename T> class ArenaEntityMap {
  static_assert(std::is_trivially_destructible_v<T>,
                "ArenaEntityMap requires trivially destructible T");

private:
  T **slots_ = nullptr;
  size_t capacity_ = 0;
  Arena *arena_ = nullptr;

public:
  ArenaEntityMap() = default;

  explicit ArenaEntityMap(Arena &arena, size_t max_entities = 4096)
      : capacity_(max_entities), arena_(&arena) {
    slots_ = arena.create_array<T *>(max_entities);
  }

  T &get_or_create(size_t id) {
    ensure_capacity(id + 1);
    if (!slots_[id]) {
      slots_[id] = arena_->create<T>();
    }
    return *slots_[id];
  }

  [[nodiscard]] T *get(size_t id) const {
    return (id < capacity_) ? slots_[id] : nullptr;
  }

  [[nodiscard]] bool contains(size_t id) const {
    return id < capacity_ && slots_[id] != nullptr;
  }

  void clear() {
    if (slots_) {
      for (size_t i = 0; i < capacity_; ++i) {
        slots_[i] = nullptr;
      }
    }
  }

private:
  void ensure_capacity(size_t needed) {
    if (needed <= capacity_)
      return;

    assert(arena_ && "ArenaEntityMap requires a valid arena");
    size_t new_capacity = capacity_ == 0 ? 256 : capacity_;
    while (new_capacity < needed) {
      new_capacity *= 2;
    }

    T **new_slots = arena_->create_array<T *>(new_capacity);
    if (slots_) {
      for (size_t i = 0; i < capacity_; ++i) {
        new_slots[i] = slots_[i];
      }
    }

    slots_ = new_slots;
    capacity_ = new_capacity;
  }
};

} // namespace afterhours

