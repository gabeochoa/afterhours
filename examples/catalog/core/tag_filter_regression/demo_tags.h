#pragma once

#include <cstdint>

#include "../../../src/core/base_component.h"

namespace afterhours {

// Shared tag/components used by regression tests and examples.
enum struct DemoTag : std::uint8_t { Runner = 0, Chaser = 1, Store = 2 };

struct TagTestTransform : BaseComponent {
    int x = 0;
};

struct TagTestHealth : BaseComponent {
    int hp = 100;
};

}  // namespace afterhours
