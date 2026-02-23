
#pragma once

#include "developer.h"
#include "plugins/color.h"

namespace afterhours {

namespace camera {
constexpr int PERSPECTIVE = 0;
constexpr int ORTHOGRAPHIC = 1;
} // namespace camera

#ifdef AFTER_HOURS_USE_RAYLIB
using Camera3D = raylib::Camera3D;
#else
struct Camera3D {
  Vector3Type position;
  Vector3Type target;
  Vector3Type up;
  float fovy;
  int projection; // 0 = perspective, 1 = orthographic
};
#endif

} // namespace afterhours

#ifdef AFTER_HOURS_USE_RAYLIB
#include "backends/raylib/drawing_helpers_3d.h"
#elif defined(AFTER_HOURS_USE_METAL)
#include "backends/sokol/drawing_helpers_3d.h"
#else
#include "backends/none/drawing_helpers_3d.h"
#endif
