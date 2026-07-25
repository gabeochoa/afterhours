#pragma once

// Random plugin.
//
// Convenience enabler: this header defines AFTER_HOURS_ENABLE_RANDOM, which
// config.h maps to the seedable-random core paths (Library::get_random_match,
// EntityQuery::gen_random). Because core headers are `#pragma once`, the define
// must land before they are first parsed -- include this at the very top of your
// PCH / afterhours include list.
//
// ODR: Library and EntityQuery are templates; their bodies must be identical in
// every TU. If some TUs see this and others don't, that's an ODR violation. For
// robust, order-independent enablement -- especially multi-TU builds without a
// shared PCH -- prefer the build flag -DAFTER_HOURS_ENABLE_RANDOM (config.h
// reads it either way). This header is the convenient path, not the safe-anywhere
// one.
#ifndef AFTER_HOURS_ENABLE_RANDOM
#define AFTER_HOURS_ENABLE_RANDOM
#endif

#include "../core/base_component.h"
#include "../developer.h"
#include "../ecs.h"
#include "../random_engine.h"

namespace afterhours {

// Ties the global RandomEngine into the ECS lifecycle and surfaces the run seed
// as a singleton component (so it is visible / serializable per run). Seeded
// random itself lives in RandomEngine; call random_plugin::set_seed() once at
// run start for a reproducible sequence.
struct random_plugin : developer::Plugin {
  struct HasRandomSeed : BaseComponent {
    std::string seed;
  };

  static void set_seed(const std::string &seed) {
    RandomEngine::get().set_seed(seed);
    if (auto *state = EntityHelper::get_singleton_cmp<HasRandomSeed>())
      state->seed = seed;
  }

  static void add_singleton_components(Entity &entity) {
    entity.addComponent<HasRandomSeed>();
    EntityHelper::registerSingleton<HasRandomSeed>(entity);
  }

  static void enforce_singletons(SystemManager &sm) {
    sm.register_update_system(
        std::make_unique<developer::EnforceSingleton<HasRandomSeed>>());
  }

  // No per-frame work; RNG is pull-based.
  static void register_update_systems(SystemManager &) {}
};

} // namespace afterhours
