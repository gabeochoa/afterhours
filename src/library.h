#pragma once

#include "config.h"

#if __has_include(<expected>)
#include <expected>
#if defined(__cpp_lib_expected)
namespace ah_std = std;
#else
#include "../expected.hpp"
namespace ah_std = tl; // fallback to tl::expected/unexpected
#endif
#else
#include "../expected.hpp"
namespace ah_std = tl; // fallback to tl::expected/unexpected
#endif
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "logging.h"
#include "type_name.h"

// Opt-in via config.h (AFTER_HOURS_ENABLE_RANDOM): make get_random_match
// actually random (and seedable). Off => no <random>/RandomEngine cost.
#if AFTER_HOURS_RANDOM_ENABLED
#include "random_engine.h"
#endif

template <typename T> struct Library {
  enum struct Error {
    DUPLICATE_NAME,
    NO_MATCH,
  };

  using Storage = std::map<std::string, T>;
  using iterator = typename Storage::iterator;
  using const_iterator = typename Storage::const_iterator;

  using Handle = uint32_t;
  static constexpr Handle invalid_handle = UINT32_MAX;

  Storage storage;

 private:
  std::unordered_map<std::string, Handle> handle_by_name;
  std::vector<T *> by_handle;

 public:
  [[nodiscard]] Handle intern(const std::string &name) {
    auto [it, inserted] = handle_by_name.emplace(name, Handle{});
    if (inserted) {
      it->second = static_cast<Handle>(by_handle.size());
      by_handle.push_back(nullptr);
    }
    return it->second;
  }

  [[nodiscard]] Handle intern(const char *name) {
    return intern(std::string(name ? name : ""));
  }

  [[nodiscard]] auto size() const { return storage.size(); }
  [[nodiscard]] auto begin() const { return storage.begin(); }
  [[nodiscard]] auto end() const { return storage.end(); }
  [[nodiscard]] auto begin() { return storage.begin(); }
  [[nodiscard]] auto end() { return storage.end(); }
  [[nodiscard]] auto rbegin() const { return storage.rbegin(); }
  [[nodiscard]] auto rend() const { return storage.rend(); }
  [[nodiscard]] auto rbegin() { return storage.rbegin(); }
  [[nodiscard]] auto rend() { return storage.rend(); }
  [[nodiscard]] auto empty() const { return storage.empty(); }

  ah_std::expected<std::string, Error> add(const char *name, const T &item) {
    log_trace("adding {} to the library", name);
    if (storage.find(name) != storage.end()) {
      return ah_std::unexpected(Error::DUPLICATE_NAME);
    }
    storage[name] = item;
    by_handle[intern(name)] = &storage[name];
    return name;
  }

  [[nodiscard]] T &get(Handle handle) {
    if (!contains(handle)) {
      log_warn("asking for an item by unknown handle {} for {}", handle,
               type_name<T>());
      throw std::out_of_range("Library: unknown handle");
    }
    return *by_handle.at(handle);
  }

  [[nodiscard]] const T &get(Handle handle) const {
    if (!contains(handle)) {
      log_warn("asking for an item by unknown handle {} for {}", handle,
               type_name<T>());
      throw std::out_of_range("Library: unknown handle");
    }
    return *by_handle.at(handle);
  }

  [[nodiscard]] bool contains(Handle handle) const {
    return handle < by_handle.size() && by_handle[handle] != nullptr;
  }

  void reset_handles() { by_handle.assign(by_handle.size(), nullptr); }

  [[nodiscard]] T &get(const std::string &name) {
    if (!this->contains(name)) {
      log_warn("asking for item: {} but nothing has been loaded with that "
               "name yet {}",
               name.c_str(), type_name<T>());
    }
    return storage.at(name);
  }

  [[nodiscard]] const T &get(const std::string &name) const {
    if (!this->contains(name)) {
      log_warn("asking for item: {} but nothing has been loaded with that "
               "name yet for {}",
               name.c_str(), type_name<T>());
    }
    return storage.at(name);
  }

  [[nodiscard]] bool contains(const std::string &name) const {
    return (storage.find(name) != storage.end());
  }

  virtual void load(const char *filename, const char *name) {
    log_trace("Loading {}: {} from {}", type_name<T>(), name, filename);
    // Check before converting: a duplicate used to be parsed, then dropped by
    // add() without unload, leaking it.
    if (contains(name)) {
      log_warn("{} '{}' is already loaded, skipping {}", type_name<T>(), name,
               filename);
      return;
    }
    this->add(name, convert_filename_to_object(name, filename));
  }

  virtual T convert_filename_to_object(const char *name,
                                       const char *filename) = 0;

  void unload_all() {
    log_info("Library<{}> loaded {} items", type_name<T>(), storage.size());
    for (auto kv : storage) {
      unload(kv.second);
    }
    reset_handles();
  }
  virtual void unload(T) = 0;

  [[nodiscard]] ah_std::expected<T, Error>
  get_random_match(const std::string &key) const {
    auto matches = lookup(key);
    size_t num_matches =
        static_cast<size_t>(std::distance(matches.first, matches.second));
    if (num_matches == 0) {
      return ah_std::unexpected(Error::NO_MATCH);
    }

    size_t idx = 0;
#if AFTER_HOURS_RANDOM_ENABLED
    if (num_matches > 1) {
      idx = static_cast<size_t>(afterhours::RandomEngine::get().get_int(
          0, static_cast<int>(num_matches) - 1));
    }
#endif
    const_iterator start(matches.first);
    std::advance(start, idx);
    return start->second;
  }

  [[nodiscard]] auto lookup(const std::string &key) const
      -> std::pair<const_iterator, const_iterator> {
    auto p = storage.lower_bound(key);
    auto q = storage.end();
    if (p != q && p->first == key) {
      return std::make_pair(p, std::next(p));
    } else {
      auto r = p;
      while (r != q && r->first.compare(0, key.size(), key) == 0) {
        ++r;
      }
      return std::make_pair(p, r);
    }
  }
};
