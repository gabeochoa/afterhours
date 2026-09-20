#pragma once

#include "../../core/system.h"
#include "../../developer.h"
#include "store.h"

namespace afterhours {

struct animation : developer::Plugin {
  using Spring = motion::Spring;
  using Timeline = motion::Timeline;
  using Mode = motion::Mode;
  template <typename T> using Track = motion::Track<T>;

  static void set_instant(bool on) { motion::set_instant(on); }
  static bool is_instant() { return motion::is_instant(); }

  static void add_singleton_components(Entity &) {}
  static void enforce_singletons(SystemManager &) {}
  static void register_update_systems(SystemManager &sm) {
    motion::register_update_systems(sm);
  }
};

static_assert(developer::PluginCore<animation>,
              "animation must implement the core plugin interface");

} // namespace afterhours
