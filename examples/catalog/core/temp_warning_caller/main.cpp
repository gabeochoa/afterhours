#include <cassert>
#include <cstdio>

#define AFTER_HOURS_ENTITY_HELPER
#define AFTER_HOURS_ENTITY_QUERY
#include "../../../../ah.h"

using namespace afterhours;

struct Position : BaseComponent {
    float x = 0.f;
    float y = 0.f;
    Position(float x_ = 0.f, float y_ = 0.f) : x(x_), y(y_) {}
};

int main() {
    EntityCollection ec;

    ec.createEntity().addComponent<Position>(1.f, 2.f);

    std::printf("querying before the merge warns and names this call site:\n");
    const auto missed = EntityQuery(ec).gen();
    std::printf("  -> %zu entities (the new one is still in temp)\n",
                missed.size());
    assert(missed.empty());

    ec.merge_entity_arrays();
    const auto found = EntityQuery(ec).whereHasComponent<Position>().gen();
    std::printf("after the merge: %zu entities, no warning\n", found.size());
    assert(found.size() == 1);

    ec.createEntity().addComponent<Position>(3.f, 4.f);
    const auto intentional =
        EntityQuery(ec, {.ignore_temp_warning = true}).gen();
    std::printf("with ignore_temp_warning: %zu entities, no warning\n",
                intentional.size());
    assert(intentional.size() == 1);
    ec.merge_entity_arrays();

    std::printf("done\n");
    return 0;
}
