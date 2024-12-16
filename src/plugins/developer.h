
#pragma once

#include <cassert>
#include <iostream>

#include "../base_component.h"
#include "../entity_query.h"
#include "../system.h"
#include "../type_name.h"

namespace afterhours {

namespace developer {

template <typename Component> struct EnforceSingleton : System<Component> {

  bool saw_one;

  virtual void once(float) override { saw_one = false; }

  virtual void for_each_with(Entity &, Component &, float) override {

    if (saw_one) {
      std::cerr << "Enforcing only one entity with " << type_name<Component>()
                << std::endl;
      assert(false);
    }
    saw_one = true;
  }
};
} // namespace developer

} // namespace afterhours
