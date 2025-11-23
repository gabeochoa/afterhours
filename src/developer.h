
#pragma once

#include <cassert>
#include <iostream>

#include "core/base_component.h"
#include "core/entity_query.h"
#include "core/system.h"
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
    bool operator<(const MyVec2 &other) const {
        if (x != other.x) return x < other.x;
        return y < other.y;
    }
    bool operator==(const MyVec2 &other) const {
        return x == other.x && y == other.y;
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
template<class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

template<typename T>
int sgn(const T val) {
    return (T(0) < val) - (val < T(0));
}

}  // namespace util

namespace developer {

template<typename Component>
struct EnforceSingleton : System<Component> {
    bool saw_one;

    virtual void once(const float) override { saw_one = false; }

    virtual void for_each_with(Entity &, Component &, const float) override {
        if (saw_one) {
            std::cerr << "Enforcing only one entity with "
                      << type_name<Component>() << std::endl;
            assert(false);
        }
        saw_one = true;
    }
};

struct Plugin {
    // Plugin Interface for afterhours ECS
    //
    // Plugins must inherit from this struct and implement the lifecycle methods
    // below. All plugins must use only public EntityHelper APIs - see
    // PLUGIN_API.md for details.
    //
    // Lifecycle:
    // 1. add_singleton_components() - Called once during initialization
    // 2. enforce_singletons() - Called to register singleton enforcement
    // systems
    // 3. register_update_systems() - Called to register all update systems

    // Called once during initialization to add singleton components to the
    // manager entity. Plugins should:
    // - Add singleton components to the provided entity
    // - Register singletons using
    // EntityHelper::registerSingleton<Component>(entity)
    // - Example:
    //     entity.addComponent<MySingleton>();
    //     EntityHelper::registerSingleton<MySingleton>(entity);
    static void add_singleton_components(Entity &entity);

    // Called to register systems that enforce singleton constraints.
    // Plugins should register EnforceSingleton<Component> systems for each
    // singleton component. Example:
    //     sm.register_update_system(
    //       std::make_unique<developer::EnforceSingleton<MySingleton>>());
    static void enforce_singletons(SystemManager &sm);

    // Called to register all update systems for this plugin.
    // Plugins should register all System<T...> subclasses that need to run each
    // frame. Example:
    //     sm.register_update_system(std::make_unique<MySystem>());
    static void register_update_systems(SystemManager &sm);
};
}  // namespace developer

}  // namespace afterhours
