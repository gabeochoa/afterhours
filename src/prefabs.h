#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/entity_collection.h"
#include "core/entity_helper.h"
#include "library.h"

namespace afterhours::prefabs {

// Builder/override: add or mutate components on the entity you are handed.
using ApplyFn = std::function<void(Entity &)>;

// Named component bundles. Registration order is preserved (see names());
// that order is the documented composition/serialize order for callers that
// walk names() themselves -- this header defines no file format.
struct Registry {
  void define(std::string name, ApplyFn builder) {
    if (!builder) return;
    if (impl.contains(name)) {
      impl.storage[name] = std::move(builder);
      return;
    }
    order.push_back(name);
    impl.add(name.c_str(), builder);
  }

  [[nodiscard]] bool has(std::string_view name) const {
    return impl.contains(std::string(name));
  }

  [[nodiscard]] std::vector<std::string> names() const { return order; }

  void clear() {
    impl.unload_all();
    impl.storage.clear();
    order.clear();
  }

  // Unknown name returns empty OptEntity and creates nothing.
  OptEntity spawn(EntityCollection &collection, std::string_view name) const {
    return spawn(collection, name, nullptr);
  }

  OptEntity spawn(EntityCollection &collection, std::string_view name,
                  const ApplyFn &overrides) const {
    if (!impl.contains(std::string(name))) return OptEntity{};
    Entity &entity = collection.createEntity();
    impl.get(std::string(name))(entity);
    if (overrides) overrides(entity);
    return OptEntity{entity};
  }

 private:
  struct Impl : Library<ApplyFn> {
    ApplyFn convert_filename_to_object(const char *, const char *) override {
      return ApplyFn{};
    }
    void unload(ApplyFn) override {}
  } impl;
  std::vector<std::string> order;
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
