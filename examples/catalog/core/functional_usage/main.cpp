

#include <iostream>

#define AFTER_HOURS_ENTITY_HELPER
#define AFTER_HOURS_ENTITY_QUERY
#define AFTER_HOURS_SYSTEM
#include "../../../example_helper.h"
#include "../../shared/vector.h"

namespace afterhours {

// Make some components :)
struct Transform : public BaseComponent {
    Transform(float x, float y) {
        position.x = x;
        position.y = y;
    }

    virtual ~Transform() {}

    [[nodiscard]] vec2 pos() const { return position; }
    void update(vec2 v) { position = v; }

   private:
    vec2 position;
};

struct OncePerTick : System<> {
    void once(float) override { std::cout << "OncePerTick::once" << std::endl; }
};

}  // namespace afterhours

void make_entities() {
    using namespace afterhours;

    for (int i = 0; i < 5; i++) {
        EntityHelper::createEntity();
    }
}

int main(int, char **) {
    using namespace afterhours;
    using namespace afterhours::examples;

    make_entities();

    SystemManager systems;
    add_update_systems(systems, [](float) { std::cout << "Anonymous::once" << std::endl; },
                       std::make_unique<OncePerTick>());

    run_frames(systems, 2, 1.f);

    return 0;
}
