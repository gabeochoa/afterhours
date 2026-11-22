#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/entity_collection.h"
#include "core/entity_helper.h"

namespace afterhours::prefabs {

// Builder/override: add or mutate components on the entity you are handed.
using ApplyFn = std::function<void(Entity &)>;

// Named component bundles. Registration order is preserved (see names());
// that order is the documented composition/serialize order for callers that
// walk names() themselves -- this header defines no file format.
struct Registry {
  void define(std::string name, ApplyFn builder) {
    if (!builder) return;
    if (builders_.find(name) == builders_.end()) order_.push_back(name);
    builders_[std::move(name)] = std::move(builder); // re-define replaces, keeps position
  }

  [[nodiscard]] bool has(std::string_view name) const {
    return builders_.find(std::string(name)) != builders_.end();
  }

  [[nodiscard]] std::vector<std::string> names() const { return order_; }
  void clear() { builders_.clear(); order_.clear(); }

  // Unknown name returns empty OptEntity and creates nothing.
  OptEntity spawn(EntityCollection &collection, std::string_view name) const {
    return spawn(collection, name, nullptr);
  }

  OptEntity spawn(EntityCollection &collection, std::string_view name,
                  const ApplyFn &overrides) const {
    auto it = builders_.find(std::string(name));
    if (it == builders_.end()) return OptEntity{};
    Entity &entity = collection.createEntity();
    it->second(entity);              // bundle defaults first
    if (overrides) overrides(entity); // caller wins: it runs last
    return OptEntity{entity};
  }

 private:
  std::unordered_map<std::string, ApplyFn> builders_;
  std::vector<std::string> order_;
};

inline Registry &registry() {
  static Registry instance;
  return instance;
}

inline void define(std::string name, ApplyFn builder) {
  registry().define(std::move(name), std::move(builder));
}

inline bool has(std::string_view name) { return registry().has(name); }
inline std::vector<std::string> names() { return registry().names(); }
inline void clear() { registry().clear(); }

inline OptEntity spawn(EntityCollection &collection, std::string_view name,
                       const ApplyFn &overrides = nullptr) {
  return registry().spawn(collection, name, overrides);
}

inline OptEntity spawn(std::string_view name,
                       const ApplyFn &overrides = nullptr) {
  return spawn(EntityHelper::get_default_collection(), name, overrides);
}

} // namespace afterhours::prefabs
