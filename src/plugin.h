#pragma once

#include <cassert>
#include <concepts>
#include <cstdio>
#include <functional>
#include <memory>
#include <string_view>
#include <utility>

#include "core/entity_helper.h"
#include "core/system.h"
#include "type_name.h"

namespace afterhours {
namespace developer {

template <typename Component> struct EnforceSingleton : System<Component> {
  bool saw_one;

  void once(const float) override { saw_one = false; }

  void for_each_with(Entity &, Component &, const float) override {
    if (saw_one) {
      const auto n = type_name<Component>();
      std::fprintf(stderr, "Enforcing only one entity with %.*s\n",
                   static_cast<int>(n.size()), n.data());
      assert(false);
    }
    saw_one = true;
  }
};

struct Plugin {
  static void add_singleton_components(Entity &entity);
  static void enforce_singletons(SystemManager &sm);
  static void register_update_systems(SystemManager &sm);
};

template <typename T>
concept PluginCore = requires(Entity &e, SystemManager &sm) {
  { T::add_singleton_components(e) } -> std::same_as<void>;
  { T::enforce_singletons(sm) } -> std::same_as<void>;
  { T::register_update_systems(sm) } -> std::same_as<void>;
};

template <typename T>
concept PluginWithRender = PluginCore<T> && requires(SystemManager &sm) {
  { T::register_render_systems(sm) } -> std::same_as<void>;
};

template <typename T, typename InputAction>
concept PluginTemplated = requires(SystemManager &sm) {
  { T::template enforce_singletons<InputAction>(sm) } -> std::same_as<void>;
  {
    T::template register_update_systems<InputAction>(sm)
  } -> std::same_as<void>;
};

template <PluginCore P> constexpr bool plugin_ok = true;

} // namespace developer

struct PluginContext {
  SystemManager &systems;
  EntityCollection &collection;
  Entity *singleton_host = nullptr;

  explicit PluginContext(
      SystemManager &sm,
      EntityCollection &coll = EntityHelper::get_default_collection())
      : systems(sm), collection(coll) {}
  PluginContext(SystemManager &sm, Entity &host)
      : systems(sm), collection(EntityHelper::get_default_collection()),
        singleton_host(&host) {}
  PluginContext(SystemManager &sm, EntityCollection &coll, Entity &host)
      : systems(sm), collection(coll), singleton_host(&host) {}

  template <typename T, typename... Args>
  T &ensure_singleton(Args &&...args) {
    if (collection.has_singleton<T>())
      return collection.get_singleton_cmp_enforce<T>();
    if (!singleton_host)
      singleton_host = &collection.createPermanentEntity();
    T &cmp = singleton_host->addComponent<T>(std::forward<Args>(args)...);
    collection.registerSingleton<T>(*singleton_host);
    collection.merge_entity_arrays();
    systems.register_system(
        Stage::PreUpdate,
        std::make_unique<developer::EnforceSingleton<T>>());
    return cmp;
  }

  void add_system(Stage stage, std::unique_ptr<SystemBase> sys,
                  std::string_view group = {}) {
    systems.register_system(stage, std::move(sys), group);
  }
  void add_system(Stage stage, const std::function<void(float)> &cb,
                  std::string_view group = {}) {
    systems.register_system(stage, cb, group);
  }

  template <typename P>
    requires requires(const P &p, PluginContext &ctx) { p.install(ctx); }
  PluginContext &use(const P &plugin) {
    plugin.install(*this);
    return *this;
  }
};

template <typename P>
concept InstallablePlugin = requires(const P &p, PluginContext &ctx) {
  { p.install(ctx) } -> std::same_as<void>;
};

} // namespace afterhours
