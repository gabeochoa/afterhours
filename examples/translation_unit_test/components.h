#pragma once

#include "../../src/entity_helper.h"
#include "../../src/system.h"

using namespace afterhours;

// Define components in a single translation unit to avoid static initialization
// order fiasco
struct Transform : BaseComponent {
  float x, y, z;
  Transform(float x = 0, float y = 0, float z = 0) : x(x), y(y), z(z) {}
};

struct Velocity : BaseComponent {
  float vx, vy, vz;
  Velocity(float vx = 0, float vy = 0, float vz = 0) : vx(vx), vy(vy), vz(vz) {}
};
