
#pragma once

#include <cassert>
#include <iostream>

#include "base_component.h"
#include "entity_query.h"
#include "system.h"
#include "type_name.h"

#ifndef TextureType
struct MyTexture {
  float width;
  float height;
};
using TextureType = MyTexture;
#endif

#ifndef RectangleType
struct MyRectangle {
  float x, y, width, height;
};
using RectangleType = MyRectangle;
#endif

#ifndef ColorType
struct MyColor {
  unsigned char r, g, b, a;
};
#define ColorType MyColor
#endif

#ifndef FontType
struct MyFont {};
#define FontType MyFont
#endif

#ifndef Vector2Type
struct MyVec2 {
  float x;
  float y;

  MyVec2 operator+(const MyVec2 &other) const {
    return MyVec2{x + other.x, y + other.y};
  }
  MyVec2 operator-(const MyVec2 &other) const {
    return MyVec2{x - other.x, y - other.y};
  }
};
using Vector2Type = MyVec2;
constexpr float distance_sq(const Vector2Type a, const Vector2Type b) {
  return (a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y);
}
#endif

namespace afterhours {

// TODO move into a dedicated file?
namespace util {
template <class... Ts> struct overloaded : Ts... {
  using Ts::operator()...;
};

template <typename T> int sgn(const T val) {
  return (T(0) < val) - (val < T(0));
}

} // namespace util

namespace developer {

template <typename Component> struct EnforceSingleton : System<Component> {

  bool saw_one;

  virtual void once(const float) override { saw_one = false; }

  virtual void for_each_with(Entity &, Component &, const float) override {

    if (saw_one) {
      std::cerr << "Enforcing only one entity with " << type_name<Component>()
                << std::endl;
      assert(false);
    }
    saw_one = true;
  }
};

struct Plugin {
  // If you are writing a plugin you should implement these functions
  // or some set/subset or these names with new parameters

  static void add_singleton_components(Entity &entity);
  static void enforce_singletons(SystemManager &sm);
  static void register_update_systems(SystemManager &sm);
};
} // namespace developer

} // namespace afterhours
